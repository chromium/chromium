// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/common/webui_url_constants.h"

HighEfficiencyBubbleDelegate::HighEfficiencyBubbleDelegate(
    Browser* browser,
    HighEfficiencyBubbleObserver* observer)
    : browser_(browser), observer_(observer) {
  DCHECK(browser);
  DCHECK(observer);
}

void HighEfficiencyBubbleDelegate::OnSettingsClicked() {
  chrome::ShowSettingsSubPage(browser_, chrome::kPerformanceSubPage);
  close_action_ = HighEfficiencyBubbleActionType::kOpenSettings;
  dialog_model()->host()->Close();
}

void HighEfficiencyBubbleDelegate::OnDialogDestroy() {
  RecordHighEfficiencyBubbleAction(close_action_);
  close_action_ = HighEfficiencyBubbleActionType::kDismiss;
  observer_->OnBubbleHidden();
}
