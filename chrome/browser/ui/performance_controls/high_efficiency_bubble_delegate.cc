// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_utils.h"
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
}

void HighEfficiencyBubbleDelegate::OnAddSiteToExceptionsListClicked() {
  content::WebContents* const web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  CHECK(web_contents);
  const std::string host = web_contents->GetURL().host();
  PrefService* const pref_service = browser_->profile()->GetPrefs();
  high_efficiency::AddSiteToExceptionsList(pref_service, host);
  close_action_ = HighEfficiencyBubbleActionType::kAddException;
}

void HighEfficiencyBubbleDelegate::OnDialogDestroy() {
  RecordHighEfficiencyBubbleAction(close_action_);
  close_action_ = HighEfficiencyBubbleActionType::kDismiss;
  observer_->OnBubbleHidden();
}
