// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/gaming_seat.h"

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
  // Disconnect all the gamepads.
  for (auto& entry : gamepads_)
    entry.second->OnRemoved();

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
        target = GetShellMainSurface(top_level_window);
    }
  }

  bool focused = target && delegate_->CanAcceptGamepadEventsForSurface(target);
  if (focused_ != focused) {
    focused_ = focused;
    if (focused) {
      ui::GamepadProviderOzone::GetInstance()->AddGamepadObserver(this);
      OnGamepadDevicesUpdated();
    } else {
      ui::GamepadProviderOzone::GetInstance()->RemoveGamepadObserver(this);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::GamepadObserver overrides:

void GamingSeat::OnGamepadDevicesUpdated() {
  std::vector<ui::GamepadDevice> gamepad_devices =
      ui::GamepadProviderOzone::GetInstance()->GetGamepadDevices();

  base::flat_map<int, GamepadDelegate*> new_gamepads;

  // Copy the "still connected gamepads".
  for (auto& device : gamepad_devices) {
    auto it = gamepads_.find(device.id);
    if (it != gamepads_.end()) {
      new_gamepads[device.id] = it->second;
      gamepads_.erase(it);
    }
  }

  // Remove each disconected gamepad.
  for (auto& entry : gamepads_)
    entry.second->OnRemoved();

  // Add each new connected gamepad.
  for (auto& device : gamepad_devices) {
    if (new_gamepads.find(device.id) == new_gamepads.end())
      new_gamepads[device.id] = delegate_->GamepadAdded(device);
  }

  new_gamepads.swap(gamepads_);
}

void GamingSeat::OnGamepadEvent(const ui::GamepadEvent& event) {
  auto it = gamepads_.find(event.device_id());
  if (it == gamepads_.end())
    return;

  switch (event.type()) {
    case ui::GamepadEventType::BUTTON:
      it->second->OnButton(event.code(), event.value());
      break;
    case ui::GamepadEventType::AXIS:
      it->second->OnAxis(event.code(), event.value());
      break;
    case ui::GamepadEventType::FRAME:
      it->second->OnFrame();
      break;
  }
}

}  // namespace exo
