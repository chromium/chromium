// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_GAMEPAD_DELEGATE_H_
#define COMPONENTS_EXO_GAMEPAD_DELEGATE_H_

#include "base/time/time.h"

namespace exo {

// Handles events for a specific gamepad.
class GamepadDelegate {
 public:
  virtual ~GamepadDelegate() = default;
  // Called when the gamepad has been removed.
  virtual void OnRemoved() = 0;

  // Called when the user moved an axis of the gamepad.
  virtual void OnAxis(int axis, double value, base::TimeTicks timestamp) = 0;

  // Called when the user pressed or moved a button of the gamepad.
  virtual void OnButton(int button,
                        bool pressed,
                        base::TimeTicks timestamp) = 0;

  // Called after all gamepad information of this frame has been set and the
  // client should evaluate the updated state.
  virtual void OnFrame(base::TimeTicks timestamp) = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_GAMEPAD_DELEGATE_H_
