// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/do_not_disturb_controller.h"

namespace chromeos {
namespace phonehub {

DoNotDisturbController::DoNotDisturbController() = default;

DoNotDisturbController::~DoNotDisturbController() = default;

void DoNotDisturbController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DoNotDisturbController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DoNotDisturbController::NotifyDndStateChanged() {
  for (auto& observer : observer_list_)
    observer.OnDndStateChanged();
}

}  // namespace phonehub
}  // namespace chromeos
