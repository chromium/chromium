// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/serial_tracker.h"

#include <sstream>

#include <wayland-server-core.h>

#include "base/logging.h"

namespace exo {
namespace wayland {

namespace {

// Number of previous events to retain information about.
constexpr uint32_t kMaxEventsTracked = 1024;

}  // namespace

// static
std::string SerialTracker::ToString(EventType type) {
  switch (type) {
    case POINTER_ENTER:
      return "pointer_enter";
    case POINTER_LEAVE:
      return "pointer_leave";
    case POINTER_LEFT_BUTTON_DOWN:
      return "left_button_down";
    case POINTER_LEFT_BUTTON_UP:
      return "left_button_up";
    case POINTER_MIDDLE_BUTTON_DOWN:
      return "middle_button_down";
    case POINTER_MIDDLE_BUTTON_UP:
      return "middle_button_up";
    case POINTER_RIGHT_BUTTON_DOWN:
      return "right_button_down";
    case POINTER_RIGHT_BUTTON_UP:
      return "right_button_up";
    case POINTER_FORWARD_BUTTON_DOWN:
      return "forward_button_down";
    case POINTER_FORWARD_BUTTON_UP:
      return "forward_button_up";
    case POINTER_BACK_BUTTON_DOWN:
      return "back_button_down";
    case POINTER_BACK_BUTTON_UP:
      return "back_button_up";
    case TOUCH_DOWN:
      return "touch_down";
    case TOUCH_UP:
      return "touch_up";
    case OTHER_EVENT:
      return "other event";
  }
}

SerialTracker::SerialTracker(struct wl_display* display)
    : display_(display), events_(kMaxEventsTracked) {}

SerialTracker::~SerialTracker() = default;

void SerialTracker::Shutdown() {
  display_ = nullptr;
}

uint32_t SerialTracker::GetNextSerial(EventType type) {
  if (!display_)
    return 0;

  uint32_t serial = wl_display_next_serial(display_);
  events_[serial % kMaxEventsTracked] = type;
  max_event_ = serial + 1;
  if ((max_event_ - min_event_) > kMaxEventsTracked)
    min_event_ = max_event_ - kMaxEventsTracked;

  return serial;
}

std::optional<SerialTracker::EventType> SerialTracker::GetEventType(
    uint32_t serial) const {
  if (max_event_ < min_event_) {
    // The valid range has partially overflowed the 32 bit space, so we should
    // only reject if the serial number is in neither the upper nor lower parts
    // of the space.
    if (!((serial < max_event_) || (serial >= min_event_))) {
      LOG(ERROR) << "Failed to find event type for serial. serial(" << serial
                 << ") is in the void range";
      return std::nullopt;
    }
  } else {
    // Normal, non-overflowed case. Reject the serial number if it isn't in the
    // interval.
    if (!((serial < max_event_) && (serial >= min_event_))) {
      LOG(ERROR) << "Failed to find event type for serial. serial(" << serial
                 << ") is outside of the range.";
      return std::nullopt;
    }
  }

  return events_[serial % kMaxEventsTracked];
}

uint32_t SerialTracker::MaybeNextKeySerial() {
  if (!key_serial_.has_value())
    key_serial_ = GetNextSerial(OTHER_EVENT);
  return key_serial_.value();
}

void SerialTracker::ResetKeySerial() {
  key_serial_ = std::nullopt;
}

std::string SerialTracker::ToString() const {
  std::ostringstream ss;
  ss << "min=" << min_event_ << ", max=" << max_event_;
  return ss.str();
}

}  // namespace wayland
}  // namespace exo
