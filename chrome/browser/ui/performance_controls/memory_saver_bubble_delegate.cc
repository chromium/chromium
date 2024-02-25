// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/memory_saver_bubble_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/memory_saver_utils.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/common/webui_url_constants.h"
#include "components/performance_manager/public/user_tuning/prefs.h"

MemorySaverBubbleDelegate::MemorySaverBubbleDelegate(
    Browser* browser,
    MemorySaverBubbleObserver* observer)
    : browser_(browser), observer_(observer) {
  DCHECK(browser);
  DCHECK(observer);
}

void MemorySaverBubbleDelegate::OnSettingsClicked() {
  chrome::ShowSettingsSubPage(browser_, chrome::kPerformanceSubPage);
  close_action_ = MemorySaverBubbleActionType::kOpenSettings;
}

void MemorySaverBubbleDelegate::OnAddSiteToTabDiscardExceptionsListClicked() {
  content::WebContents* const web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  CHECK(web_contents);
  const std::string host = web_contents->GetURL().host();
  PrefService* const pref_service = browser_->profile()->GetPrefs();
  performance_manager::user_tuning::prefs::AddSiteToTabDiscardExceptionsList(
      pref_service, host);
  close_action_ = MemorySaverBubbleActionType::kAddException;
}

void MemorySaverBubbleDelegate::OnDialogDestroy() {
  RecordMemorySaverBubbleAction(close_action_);
  close_action_ = MemorySaverBubbleActionType::kDismiss;
  observer_->OnBubbleHidden();
}
