// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_GAMEPAD_OBSERVER_H_
#define COMPONENTS_EXO_GAMEPAD_OBSERVER_H_

namespace exo {

class Gamepad;

// Observers to the gamepad are notified when the gamepad destructs.
class GamepadObserver {
 public:
  virtual ~GamepadObserver() = default;

  // Called at the top of the gamepad's destructor, to give observers a change
  // to remove themselves.
  virtual void OnGamepadDestroying(Gamepad* gamepad) = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_GAMEPAD_OBSERVER_H_
