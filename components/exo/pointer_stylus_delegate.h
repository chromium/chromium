// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_POINTER_STYLUS_DELEGATE_H_
#define COMPONENTS_EXO_POINTER_STYLUS_DELEGATE_H_

#include "base/time/time.h"
#include "ui/events/event_constants.h"

namespace gfx {
class Vector2dF;
}

namespace exo {
class Pointer;

// Handles tool-specific event details on pointers. Used as an extension to the
// PointerDelegate.
class PointerStylusDelegate {
 public:
  // Called at the top of the pointer's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnPointerDestroying(Pointer* pointer) = 0;

  // Called when the type of pointer device changes.
  virtual void OnPointerToolChange(ui::EventPointerType type) = 0;

  // Called when the force (pressure) of the pointer changes.
  // Normalized to be [0, 1]. NaN means pressure is not supported by the
  // input device.
  virtual void OnPointerForce(base::TimeTicks time_stamp, float force) = 0;

  // Called when the tilt of a pen/stylus changes. Measured from surface normal
  // as plane angle in degrees, values lie in [-90,90]. A positive x is to the
  // right and a positive y is towards the user. Always 0 if the device does
  // not support it.
  virtual void OnPointerTilt(base::TimeTicks time_stamp,
                             const gfx::Vector2dF& tilt) = 0;

 protected:
  virtual ~PointerStylusDelegate() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_POINTER_STYLUS_DELEGATE_H_
