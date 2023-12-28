// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_GAMEPAD_H_
#define COMPONENTS_EXO_GAMEPAD_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/gamepad_observer.h"
#include "ui/events/devices/gamepad_device.h"
#include "ui/events/ozone/gamepad/gamepad_event.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace exo {

// Maximum force feedback duration supported by Linux.
constexpr int64_t kMaxDurationMillis = 0xFFFF;

// This class represents one gamepad. It allows control over the gamepad's
// vibration and provides focus tracking for the gamepad.
class Gamepad {
 public:
  explicit Gamepad(const ui::GamepadDevice& gamepad_device);
  Gamepad(const Gamepad& other) = delete;
  Gamepad& operator=(const Gamepad& other) = delete;

  // The destructor also informs GamepadObservers and GamepadDelegate when a
  // gamepad has been disconnected.
  virtual ~Gamepad();

  // Controls vibration effects on the gamepad.
  // The duration_millis/amplitude pairs determine the duration and strength of
  // the vibration. Note that the two vectors have to be the same size.
  // The repeat value determines the index of the duration_millis (or
  // amplitudes) vector at which the pattern to repeat begins. If repeat is
  // enabled, the vibration pattern will repeat indefinitely until the vibration
  // event is canceled. A repeat value of -1 disables repeat.
  // The user does not have to explicitly call CancelVibration() at the end of
  // every vibration call. However, if Vibrate() is called when there is an
  // ongoing vibration, the existing vibration is automatically interrupted and
  // canceled. The gamepad has to be focused in order for the gamepad to
  // vibrate. If focus is lost when there is an ongoing vibration, the vibration
  // is canceled automatically.
  void Vibrate(const std::vector<int64_t>& duration_millis,
               const std::vector<uint8_t>& amplitudes,
               int32_t repeat);
  void CancelVibration();

  // The GamepadDelegate is not owned by Gamepad. The delegate must stay alive
  // until OnRemoved is called.
  void SetDelegate(std::unique_ptr<GamepadDelegate> delegate);

  // Manages the GamepadObserver list. GamepadObservers are notified when the
  // gamepad is being destroyed.
  void AddObserver(GamepadObserver* observer);
  bool HasObserver(GamepadObserver* observer) const;
  void RemoveObserver(GamepadObserver* observer);

  // Informs the gamepad when window focus changes; focus changes determine
  // whether a gamepad is allowed to vibrate at any given time.
  void OnGamepadFocused();
  void OnGamepadFocusLost();

  // Forwards gamepad events to the corresponding GamepadDelegate calls.
  void OnGamepadEvent(const ui::GamepadEvent& event);

  const ui::GamepadDevice device;

 private:
  // Private method for handling vibration patterns. Handles repeat and
  // breaking down of vibration events by iterating through duration/amplitude
  // vectors. Also provides handling for a vibration event that exceeds the
  // maximum force feedback duration supported by Linux.
  void HandleVibrate(const std::vector<int64_t>& duration_millis,
                     const std::vector<uint8_t>& amplitudes,
                     int32_t repeat,
                     size_t start_index,
                     int64_t duration_already_vibrated);

  // These methods forward vibration calls to |input_controller_|.
  // They are virtual for testing purposes.
  virtual void SendVibrate(uint8_t amplitude, int64_t duration_millis);
  virtual void SendCancelVibration();

  // Keeps track of whether the gamepad is allowed to vibrate at any given
  // time.
  bool can_vibrate_ = false;

  std::unique_ptr<GamepadDelegate> delegate_;

  base::ObserverList<GamepadObserver>::Unchecked observer_list_;

  // Methods to control gamepad vibration are routed through InputController.
  raw_ptr<ui::InputController> input_controller_;

  // A timer to keep track of vibration requests.
  base::OneShotTimer vibration_timer_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_GAMEPAD_H_
