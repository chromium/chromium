// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_observer.h"
#include "chrome/common/webui_url_constants.h"

HighEfficiencyBubbleDelegate::HighEfficiencyBubbleDelegate(
    Browser* browser,
    HighEfficiencyBubbleObserver* observer)
    : browser_(browser), observer_(observer) {
  DCHECK(browser);
  DCHECK(observer);
}

void HighEfficiencyBubbleDelegate::OnSettingsClicked(const ui::Event& event) {
  chrome::ShowSettingsSubPage(browser_, chrome::kPerformanceSubPage);
}

void HighEfficiencyBubbleDelegate::OnDialogDestroy() {
  observer_->OnBubbleHidden();
}
