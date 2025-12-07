// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_POINTER_GESTURE_PINCH_DELEGATE_H_
#define COMPONENTS_EXO_POINTER_GESTURE_PINCH_DELEGATE_H_

#include "base/time/time.h"

namespace exo {
class Pointer;

// Handles pinch event details on pointers. Used as an extension to the
// PointerDelegate.
class PointerGesturePinchDelegate {
 public:
  // Called at the top of the pointer's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnPointerDestroying(Pointer* pointer) = 0;

  virtual void OnPointerPinchBegin(uint32_t unique_touch_event_id,
                                   base::TimeTicks time_stamp,
                                   Surface* surface) = 0;

  virtual void OnPointerPinchUpdate(base::TimeTicks time_stamp,
                                    float scale) = 0;

  virtual void OnPointerPinchEnd(uint32_t unique_touch_event_id,
                                 base::TimeTicks time_stamp) = 0;

 protected:
  virtual ~PointerGesturePinchDelegate() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_POINTER_GESTURE_PINCH_DELEGATE_H_
