//original source: https://github.com/gregoiresage/pebble-image-viewer

#include <pebble.h>

static Window *window;

static TextLayer *s_time_layer;
static TextLayer *battery_layer;

static TextLayer    *text_layer;
static BitmapLayer  *image_layer;
static GBitmap      *image;
static uint8_t      *data_image;

#define KEY_IMAGE   0
#define KEY_INDEX   1
#define KEY_MESSAGE 2

#define CHUNK_SIZE 1500

static void cb_in_received_handler(DictionaryIterator *iter, void *context) {
  // Get the bitmap
  Tuple *image_tuple = dict_find(iter, KEY_IMAGE);
  Tuple *index_tuple = dict_find(iter, KEY_INDEX);
  if (index_tuple && image_tuple) {
    int32_t index = index_tuple->value->int32;

    APP_LOG(APP_LOG_LEVEL_DEBUG, "image received index=%ld size=%d", index, image_tuple->length);
    memcpy(data_image + index,&image_tuple->value->uint8,image_tuple->length);

    if(image_tuple->length < CHUNK_SIZE){
      if(image){
        gbitmap_destroy(image);
        image = NULL;
      }
      image = gbitmap_create_with_data(data_image);
      bitmap_layer_set_bitmap(image_layer, image);
      text_layer_set_text(text_layer, "");
      layer_mark_dirty(bitmap_layer_get_layer(image_layer));
    }
  }

  Tuple *message_tuple = dict_find(iter, KEY_MESSAGE);
  if(message_tuple){
    text_layer_set_text(text_layer, message_tuple->value->cstring);
  }
}

// Battery handler - called when battery state changes
static void battery_handler(BatteryChargeState charge_state) {
  static char s_battery_buffer[16];

  if (charge_state.is_charging) {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "charging");
  } else {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d%%", charge_state.charge_percent);
  }
  text_layer_set_text(battery_layer, s_battery_buffer);
}

// Time tick handler - helper
static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char buffer[] = "00:00";

  // Write the current hours and minutes into the buffer
  if(clock_is_24h_style() == true) {
    //Use 2h hour format
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    //Use 12 hour format
    strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
  }

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, buffer);
}

// Time tick handler
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}


static void app_message_init() {
  // Register message handlers
  app_message_register_inbox_received(cb_in_received_handler);
  // Init buffers
  app_message_open(app_message_inbox_size_maximum(), APP_MESSAGE_OUTBOX_SIZE_MINIMUM);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = GRect(0,25,144,144); //layer_get_bounds(window_layer);

  image = gbitmap_create_with_data(data_image);

  // Create GBitmap, then set to created BitmapLayer
  image_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(image_layer, image);
  bitmap_layer_set_alignment(image_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(image_layer));
  
  //TODO remove
  text_layer = text_layer_create(GRect(0, bounds.size.h - 16, bounds.size.w, 16));
  text_layer_set_text(text_layer, "Press select");
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  text_layer_set_background_color(text_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
  
  
  // ===== Create time TextLayer =====
  s_time_layer = text_layer_create(GRect(5, 0, 144, 20));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00");

  // Improve the layout to be more like a watchface
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  //text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
  
  // Make sure the time is displayed from the start
  update_time();
  
  // ===== Create battery text layer =====
  battery_layer = text_layer_create(GRect(0, 0, 140, 20));
  text_layer_set_background_color(battery_layer, GColorClear);
  text_layer_set_text_color(battery_layer, GColorBlack);
  //text_layer_set_text(battery_layer, "0%");

  // Improve the layout to be more like a watchface
  text_layer_set_font(battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(battery_layer, GTextAlignmentRight);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(battery_layer));
  
  // Initialize with the current charge
  static char s_battery_buffer[16];
  BatteryChargeState charge_state = battery_state_service_peek();
  if (charge_state.is_charging) {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "charging");
  } else {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d%%", charge_state.charge_percent);
  }
  text_layer_set_text(battery_layer, s_battery_buffer);
  
  
  
  //text_layer_set_text(text_layer, "Updating image...");
  text_layer_set_text(text_layer, "Getting Sipper pass...");

  DictionaryIterator *iter;
  uint8_t value = 1;
  app_message_outbox_begin(&iter);
  dict_write_int(iter, KEY_IMAGE, &value, 1, true);
  dict_write_end(iter);
  app_message_outbox_send();
  
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
  bitmap_layer_destroy(image_layer);
  if(image){
    gbitmap_destroy(image);
    image = NULL;
  }
}

static void init(void) {
  app_message_init();
  data_image = malloc(sizeof(uint8_t) * (5 * 4) * 168 + 12);
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  //window_set_fullscreen(window, true);
  window_stack_push(window, true);
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Register battery handler
  battery_state_service_subscribe(battery_handler);
}

static void deinit(void) {
  window_destroy(window);
  free(data_image);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
