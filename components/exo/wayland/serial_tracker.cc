// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/serial_tracker.h"

#include <wayland-server-core.h>

namespace exo {
namespace wayland {

namespace {

// Number of previous events to retain information about.
constexpr uint32_t kMaxEventsTracked = 1024;

}  // namespace

SerialTracker::SerialTracker(struct wl_display* display)
    : display_(display), events_(kMaxEventsTracked) {}

SerialTracker::~SerialTracker() {}

uint32_t SerialTracker::GetNextSerial(EventType type) {
  uint32_t serial = wl_display_next_serial(display_);
  events_[serial % kMaxEventsTracked] = type;
  max_event_ = serial + 1;
  if ((max_event_ - min_event_) > kMaxEventsTracked)
    min_event_ = max_event_ - kMaxEventsTracked;

  switch (type) {
    case EventType::POINTER_BUTTON_DOWN:
      pointer_down_serial_ = serial;
      break;
    case EventType::POINTER_BUTTON_UP:
      pointer_down_serial_ = base::nullopt;
      break;
    case EventType::TOUCH_DOWN:
      touch_down_serial_ = serial;
      break;
    case EventType::TOUCH_UP:
      touch_down_serial_ = base::nullopt;
      break;
    default:
      break;
  }

  return serial;
}

base::Optional<SerialTracker::EventType> SerialTracker::GetEventType(
    uint32_t serial) const {
  if (max_event_ < min_event_) {
    // The valid range has partially overflowed the 32 bit space, so we should
    // only reject if the serial number is in neither the upper nor lower parts
    // of the space.
    if (!((serial < max_event_) || (serial >= min_event_)))
      return base::nullopt;
  } else {
    // Normal, non-overflowed case. Reject the serial number if it isn't in the
    // interval.
    if (!((serial < max_event_) && (serial >= min_event_)))
      return base::nullopt;
  }

  return events_[serial % kMaxEventsTracked];
}

base::Optional<uint32_t> SerialTracker::GetPointerDownSerial() {
  return pointer_down_serial_;
}

base::Optional<uint32_t> SerialTracker::GetTouchDownSerial() {
  return touch_down_serial_;
}

void SerialTracker::ResetTouchDownSerial() {
  touch_down_serial_ = base::nullopt;
}

}  // namespace wayland
}  // namespace exo
