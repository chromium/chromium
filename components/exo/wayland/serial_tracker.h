// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_SERIAL_TRACKER_H_
#define COMPONENTS_EXO_WAYLAND_SERIAL_TRACKER_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"

struct wl_display;

namespace exo {
namespace wayland {

class SerialTracker {
 public:
  enum EventType {
    POINTER_ENTER,
    POINTER_LEAVE,
    POINTER_LEFT_BUTTON_DOWN,
    POINTER_LEFT_BUTTON_UP,
    POINTER_MIDDLE_BUTTON_DOWN,
    POINTER_MIDDLE_BUTTON_UP,
    POINTER_RIGHT_BUTTON_DOWN,
    POINTER_RIGHT_BUTTON_UP,
    POINTER_FORWARD_BUTTON_DOWN,
    POINTER_FORWARD_BUTTON_UP,
    POINTER_BACK_BUTTON_DOWN,
    POINTER_BACK_BUTTON_UP,
    TOUCH_DOWN,
    TOUCH_UP,
    OTHER_EVENT,
  };

  static std::string ToString(EventType type);

  explicit SerialTracker(struct wl_display* display);
  SerialTracker(const SerialTracker&) = delete;
  SerialTracker& operator=(const SerialTracker&) = delete;
  ~SerialTracker();

  // After shutdown, |GetNextSerial| returns 0.
  void Shutdown();

  uint32_t GetNextSerial(EventType type);

  // If there exists a serial for key already, returns it. Or, it creates
  // a new serial, and returns it.
  uint32_t MaybeNextKeySerial();

  // Resets the stored key serial, so that next MaybeNextKeySerial() call will
  // generate a new serial.
  void ResetKeySerial();

  // Get the EventType for a serial number, or nullopt if the serial number was
  // never sent or is too old.
  std::optional<EventType> GetEventType(uint32_t serial) const;

  std::string ToString() const;

 private:
  raw_ptr<struct wl_display, DanglingUntriaged> display_;

  // EventTypes are stored in a circular buffer, because serial numbers are
  // issued sequentially and we only want to store the most recent events.
  std::vector<EventType> events_;

  // [min_event_, max_event) is a half-open interval containing the range of
  // valid serial numbers. Note that as serial numbers are allowed to wrap
  // around the 32 bit space, we cannot assume that max_event_ >= min_event_.
  uint32_t min_event_ = 1;
  uint32_t max_event_ = 1;

  std::optional<uint32_t> key_serial_;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_SERIAL_TRACKER_H_
