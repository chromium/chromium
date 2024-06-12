// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/input_router_config_helper.h"

#include "base/command_line.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gesture_detection/gesture_detector.h"

namespace input {
namespace {

PassthroughTouchEventQueue::Config CreateTouchEventQueueConfig(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  PassthroughTouchEventQueue::Config config;

#if BUILDFLAG(IS_ANDROID)
  // For historical reasons only Android enables the touch ack timeout.
  config.touch_ack_timeout_supported = true;
#else
  config.touch_ack_timeout_supported = false;
#endif
  config.task_runner = task_runner;
  return config;
}

GestureEventQueue::Config GetGestureEventQueueConfig() {
  GestureEventQueue::Config config;
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  config.debounce_interval =
      base::Milliseconds(gesture_config->scroll_debounce_interval_in_ms());

  config.fling_config.touchscreen_tap_suppression_config.enabled =
      gesture_config->fling_touchscreen_tap_suppression_enabled();
  config.fling_config.touchscreen_tap_suppression_config
      .max_cancel_to_down_time =
      base::Milliseconds(gesture_config->fling_max_cancel_to_down_time_in_ms());

  config.fling_config.touchpad_tap_suppression_config.enabled =
      gesture_config->fling_touchpad_tap_suppression_enabled();
  config.fling_config.touchpad_tap_suppression_config.max_cancel_to_down_time =
      base::Milliseconds(gesture_config->fling_max_cancel_to_down_time_in_ms());

  return config;
}

}  // namespace

InputRouter::Config::Config() {}

InputRouter::Config GetInputRouterConfigForPlatform(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  InputRouter::Config config;
  config.gesture_config = GetGestureEventQueueConfig();
  config.touch_config = CreateTouchEventQueueConfig(std::move(task_runner));
  return config;
}

}  // namespace input
