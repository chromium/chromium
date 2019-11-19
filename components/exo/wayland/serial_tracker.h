// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_SERIAL_TRACKER_H_
#define COMPONENTS_EXO_WAYLAND_SERIAL_TRACKER_H_

#include <map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"

struct wl_display;

namespace exo {
namespace wayland {

class SerialTracker {
 public:
  enum EventType {
    POINTER_ENTER,
    POINTER_LEAVE,
    POINTER_BUTTON_DOWN,
    POINTER_BUTTON_UP,

    TOUCH_DOWN,
    TOUCH_UP,

    OTHER_EVENT,
  };

  explicit SerialTracker(struct wl_display* display);
  ~SerialTracker();

  uint32_t GetNextSerial(EventType type);

  // Get the serial number of the last {pointer,touch} pressed event, or nullopt
  // if the press has since been released.
  base::Optional<uint32_t> GetPointerDownSerial();
  base::Optional<uint32_t> GetTouchDownSerial();

  // Needed because wl_touch::cancel doesn't send a serial number, so we can't
  // test for it in GetNextSerial.
  void ResetTouchDownSerial();

  // Get the EventType for a serial number, or nullopt if the serial number was
  // never sent or is too old.
  base::Optional<EventType> GetEventType(uint32_t serial) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SerialTrackerTest, WrapAroundWholeRange);

  struct wl_display* const display_;

  // EventTypes are stored in a circular buffer, because serial numbers are
  // issued sequentially and we only want to store the most recent events.
  std::vector<EventType> events_;

  // [min_event_, max_event) is a half-open interval containing the range of
  // valid serial numbers. Note that as serial numbers are allowed to wrap
  // around the 32 bit space, we cannot assume that max_event_ >= min_event_.
  uint32_t min_event_ = 1;
  uint32_t max_event_ = 1;

  base::Optional<uint32_t> pointer_down_serial_;
  base::Optional<uint32_t> touch_down_serial_;

  DISALLOW_COPY_AND_ASSIGN(SerialTracker);
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_SERIAL_TRACKER_H_
