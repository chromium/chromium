// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/gamepad.h"

#include "base/logging.h"

namespace exo {

Gamepad::Gamepad(const ui::GamepadDevice& gamepad_device)
    : device(gamepad_device) {}

Gamepad::~Gamepad() {
  for (GamepadObserver& observer : observer_list_)
    observer.OnGamepadDestroying(this);

  if (delegate_)
    delegate_->OnRemoved();
}

void Gamepad::SetDelegate(std::unique_ptr<GamepadDelegate> delegate) {
  DCHECK(!delegate_);
  delegate_ = std::move(delegate);
}

void Gamepad::AddObserver(GamepadObserver* observer) {
  observer_list_.AddObserver(observer);
}

bool Gamepad::HasObserver(GamepadObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

void Gamepad::RemoveObserver(GamepadObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void Gamepad::OnGamepadEvent(const ui::GamepadEvent& event) {
  DCHECK(delegate_);
  switch (event.type()) {
    case ui::GamepadEventType::BUTTON:
      delegate_->OnButton(event.code(), event.value(), event.timestamp());
      break;
    case ui::GamepadEventType::AXIS:
      delegate_->OnAxis(event.code(), event.value(), event.timestamp());
      break;
    case ui::GamepadEventType::FRAME:
      delegate_->OnFrame(event.timestamp());
      break;
  }
}

}  // namespace exo
