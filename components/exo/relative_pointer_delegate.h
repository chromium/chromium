// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_RELATIVE_POINTER_DELEGATE_H_
#define COMPONENTS_EXO_RELATIVE_POINTER_DELEGATE_H_

#include "base/time/time.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace exo {
class Pointer;

// Handles sending relative mouse movements.
class RelativePointerDelegate {
 public:
  // Called at the top of the pointer's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnPointerDestroying(Pointer* pointer) = 0;

  // Called to indicate motion. |relative_motion| is given in screen-terms,
  // whereas |ordinal_motion| comes from hardware and e.g. is not accelerated.
  virtual void OnPointerRelativeMotion(
      base::TimeTicks time_stamp,
      const gfx::Vector2dF& relative_motion,
      const gfx::Vector2dF& ordinal_motion) = 0;

 protected:
  virtual ~RelativePointerDelegate() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_RELATIVE_POINTER_DELEGATE_H_
