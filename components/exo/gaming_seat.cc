// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/gaming_seat.h"

#include <vector>

#include "components/exo/gamepad.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/gaming_seat_delegate.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// GamingSeat, public:

GamingSeat::GamingSeat(GamingSeatDelegate* delegate) : delegate_(delegate) {
  auto* helper = WMHelper::GetInstance();
  helper->AddFocusObserver(this);
  OnWindowFocused(helper->GetFocusedWindow(), nullptr);
}

GamingSeat::~GamingSeat() {
  if (focused_)
    ui::GamepadProviderOzone::GetInstance()->RemoveGamepadObserver(this);
  delegate_->OnGamingSeatDestroying(this);

  WMHelper::GetInstance()->RemoveFocusObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// ui::aura::client::FocusChangeObserver overrides:

void GamingSeat::OnWindowFocused(aura::Window* gained_focus,
                                 aura::Window* lost_focus) {
  Surface* target = nullptr;
  if (gained_focus) {
    target = Surface::AsSurface(gained_focus);
    if (!target) {
      aura::Window* top_level_window = gained_focus->GetToplevelWindow();
      if (top_level_window)
        target = GetShellRootSurface(top_level_window);
    }
  }

  bool focused = target && delegate_->CanAcceptGamepadEventsForSurface(target);
  if (focused_ != focused) {
    focused_ = focused;
    if (focused) {
      ui::GamepadProviderOzone::GetInstance()->AddGamepadObserver(this);
      OnGamepadDevicesUpdated();
      for (auto& entry : gamepads_)
        entry.second->OnGamepadFocused();
    } else {
      ui::GamepadProviderOzone::GetInstance()->RemoveGamepadObserver(this);
      for (auto& entry : gamepads_)
        entry.second->OnGamepadFocusLost();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::GamepadObserver overrides:

void GamingSeat::OnGamepadDevicesUpdated() {
  std::vector<ui::GamepadDevice> gamepad_devices =
      ui::GamepadProviderOzone::GetInstance()->GetGamepadDevices();

  base::flat_map<int, std::unique_ptr<Gamepad>> new_gamepads;

  // Copy the "still connected gamepads".
  for (auto& device : gamepad_devices) {
    auto it = gamepads_.find(device.id);
    if (it != gamepads_.end()) {
      new_gamepads[device.id] = std::move(it->second);
      gamepads_.erase(it);
    }
  }

  // Add each new connected gamepad.
  for (auto& device : gamepad_devices) {
    if (new_gamepads.find(device.id) == new_gamepads.end()) {
      std::unique_ptr<Gamepad> gamepad = std::make_unique<Gamepad>(device);
      if (focused_)
        gamepad->OnGamepadFocused();
      delegate_->GamepadAdded(*gamepad);
      new_gamepads[device.id] = std::move(gamepad);
    }
  }

  new_gamepads.swap(gamepads_);
}

void GamingSeat::OnGamepadEvent(const ui::GamepadEvent& event) {
  auto it = gamepads_.find(event.device_id());
  if (it == gamepads_.end())
    return;

  it->second->OnGamepadEvent(event);
}

}  // namespace exo
