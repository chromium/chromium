// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/battery_saver_button_controller.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/performance_controls/battery_saver_button_controller_delegate.h"

BatterySaverButtonController::BatterySaverButtonController() = default;

BatterySaverButtonController::~BatterySaverButtonController() = default;

void BatterySaverButtonController::Init(
    BatterySaverButtonControllerDelegate* delegate) {
  DCHECK(delegate);
  DCHECK(!battery_saver_observer_.IsObserving());

  delegate_ = delegate;

  auto* manager =
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance();
  battery_saver_observer_.Observe(manager);

  // Update the initial state of the delegate based on the current state
  bool is_active = manager->IsBatterySaverActive();
  UpdateVisibilityState(is_active);
}

void BatterySaverButtonController::OnBatterySaverActiveChanged(bool is_active) {
  UpdateVisibilityState(is_active);
}

void BatterySaverButtonController::UpdateVisibilityState(bool is_active) {
  if (is_active)
    delegate_->Show();
  else
    delegate_->Hide();
}
