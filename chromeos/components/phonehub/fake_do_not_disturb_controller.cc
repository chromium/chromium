// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_do_not_disturb_controller.h"

namespace chromeos {
namespace phonehub {

FakeDoNotDisturbController::FakeDoNotDisturbController() = default;

FakeDoNotDisturbController::~FakeDoNotDisturbController() = default;

bool FakeDoNotDisturbController::IsDndEnabled() const {
  return is_dnd_enabled_;
}

void FakeDoNotDisturbController::SetDoNotDisturbState(bool enabled) {
  if (is_dnd_enabled_ == enabled)
    return;

  is_dnd_enabled_ = enabled;
  NotifyDndStateChanged();
}

}  // namespace phonehub
}  // namespace chromeos
