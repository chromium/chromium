// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/battery_saver_bubble_delegate.h"

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
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
}

void BatterySaverBubbleDelegate::OnSessionOffClicked(const ui::Event& event) {
  auto* manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  manager->SetTemporaryBatterySaverDisabledForSession(true);
}

void BatterySaverBubbleDelegate::OnSettingsClicked() {
  chrome::ShowSettingsSubPage(browser_, chrome::kPerformanceSubPage);
  dialog_model()->host()->Close();
}
