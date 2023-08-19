// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/battery_saver_bubble_delegate.h"

#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/performance_controls/battery_saver_bubble_observer.h"
#include "chrome/common/webui_url_constants.h"

BatterySaverBubbleDelegate::BatterySaverBubbleDelegate(
    Browser* browser,
    BatterySaverBubbleObserver* observer)
    : browser_(browser), observer_(observer) {
  DCHECK(browser);
  DCHECK(observer);
}

void BatterySaverBubbleDelegate::OnWindowClosing() {
  observer_->OnBubbleHidden();
  RecordBatterySaverBubbleAction(action_type_);
}

void BatterySaverBubbleDelegate::OnSessionOffClicked() {
  action_type_ = BatterySaverBubbleActionType::kTurnOffNow;
  auto* manager =
      performance_manager::user_tuning::BatterySaverModeManager::GetInstance();
  manager->SetTemporaryBatterySaverDisabledForSession(true);
}
