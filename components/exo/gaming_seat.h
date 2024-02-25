// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_GAMING_SEAT_H_
#define COMPONENTS_EXO_GAMING_SEAT_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "components/exo/gamepad.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/events/ozone/gamepad/gamepad_observer.h"

namespace exo {
class GamingSeatDelegate;
class GamepadDelegate;

// This class represents one gaming seat. It uses /device/gamepad or
// ozone/gamepad as backend and notifies corresponding GamepadDelegate of any
// gamepad changes.
class GamingSeat : public aura::client::FocusChangeObserver,
                   public ui::GamepadObserver {
 public:
  // This class will monitor gamepad connection changes and manage gamepads.
  GamingSeat(GamingSeatDelegate* gaming_seat_delegate);

  GamingSeat(const GamingSeat&) = delete;
  GamingSeat& operator=(const GamingSeat&) = delete;

  ~GamingSeat() override;

  // Overridden from ui::aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from ui::GamepadObserver:
  void OnGamepadDevicesUpdated() override;
  void OnGamepadEvent(const ui::GamepadEvent& event) override;

 private:
  // The delegate that handles gamepad_added.
  const raw_ptr<GamingSeatDelegate, DanglingUntriaged> delegate_;

  // Contains the delegate for each gamepad device.
  base::flat_map<int, std::unique_ptr<Gamepad>> gamepads_;

  // The flag if a valid target for gaming seat is focused. In other words, if
  // it's true, this class is observing gamepad events.
  bool focused_ = false;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_GAMING_SEAT_H_
