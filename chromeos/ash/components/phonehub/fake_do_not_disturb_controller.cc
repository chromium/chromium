// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_do_not_disturb_controller.h"

namespace ash {
namespace phonehub {

FakeDoNotDisturbController::FakeDoNotDisturbController() = default;

FakeDoNotDisturbController::~FakeDoNotDisturbController() = default;

bool FakeDoNotDisturbController::IsDndEnabled() const {
  return is_dnd_enabled_;
}

void FakeDoNotDisturbController::SetDoNotDisturbStateInternal(
    bool is_dnd_enabled,
    bool can_request_new_dnd_state) {
  if (is_dnd_enabled_ == is_dnd_enabled &&
      can_request_new_dnd_state_ == can_request_new_dnd_state) {
    return;
  }

  is_dnd_enabled_ = is_dnd_enabled;
  can_request_new_dnd_state_ = can_request_new_dnd_state;

  NotifyDndStateChanged();
}

void FakeDoNotDisturbController::RequestNewDoNotDisturbState(bool enabled) {
  if (!should_request_fail_)
    SetDoNotDisturbStateInternal(enabled, /*can_request_new_dnd_state=*/true);
}

bool FakeDoNotDisturbController::CanRequestNewDndState() const {
  return can_request_new_dnd_state_;
}

void FakeDoNotDisturbController::SetShouldRequestFail(
    bool should_request_fail) {
  should_request_fail_ = should_request_fail;
}

}  // namespace phonehub
}  // namespace ash
