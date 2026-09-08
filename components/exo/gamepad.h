// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_GAMEPAD_H_
#define COMPONENTS_EXO_GAMEPAD_H_

#include <vector>

#include "base/observer_list.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/gamepad_observer.h"
#include "ui/events/devices/gamepad_device.h"
#include "ui/events/ozone/gamepad/gamepad_event.h"

namespace exo {

// This class represents one gamepad.
class Gamepad {
 public:
  explicit Gamepad(const ui::GamepadDevice& gamepad_device);
  Gamepad(const Gamepad& other) = delete;
  Gamepad& operator=(const Gamepad& other) = delete;

  // The destructor also informs GamepadObservers and GamepadDelegate when a
  // gamepad has been disconnected.
  virtual ~Gamepad();

  // The GamepadDelegate is not owned by Gamepad. The delegate must stay alive
  // until OnRemoved is called.
  void SetDelegate(std::unique_ptr<GamepadDelegate> delegate);

  // Manages the GamepadObserver list. GamepadObservers are notified when the
  // gamepad is being destroyed.
  void AddObserver(GamepadObserver* observer);
  bool HasObserver(GamepadObserver* observer) const;
  void RemoveObserver(GamepadObserver* observer);

  // Forwards gamepad events to the corresponding GamepadDelegate calls.
  void OnGamepadEvent(const ui::GamepadEvent& event);

  const ui::GamepadDevice device;

 private:
  std::unique_ptr<GamepadDelegate> delegate_;

  base::ObserverList<GamepadObserver>::Unchecked observer_list_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_GAMEPAD_H_
