// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_manager.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_delegate.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"

using CloseReason = DefaultBrowserPromptManager::CloseReason;

DefaultBrowserInfoBarManager::DefaultBrowserInfoBarManager() = default;
DefaultBrowserInfoBarManager::~DefaultBrowserInfoBarManager() = default;

void DefaultBrowserInfoBarManager::ShowInfoBars(bool can_pin_to_taskbar) {
  can_pin_to_taskbar_ = can_pin_to_taskbar;
  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, this);
  // This will trigger a call to `OnTabStripModelChanged`, which will create
  // the info bar.
  browser_tab_strip_tracker_->Init();
}

void DefaultBrowserInfoBarManager::CloseAllInfoBars() {
  can_pin_to_taskbar_ = false;
  user_initiated_info_bar_close_pending_.reset();
  browser_tab_strip_tracker_.reset();

  for (const auto& infobars_entry : infobars_) {
    infobars_entry.second->owner()->RemoveObserver(this);
    infobars_entry.second->RemoveSelf();
  }

  infobars_.clear();
}

void DefaultBrowserInfoBarManager::CreateInfoBarForWebContents(
    content::WebContents* web_contents,
    Profile* profile) {
  // Ensure that an infobar hasn't already been created.
  CHECK(!infobars_.contains(web_contents));

  infobars::InfoBar* infobar = DefaultBrowserInfoBarDelegate::Create(
      infobars::ContentInfoBarManager::FromWebContents(web_contents), profile,
      can_pin_to_taskbar_);

  if (infobar == nullptr) {
    // Infobar may be null if `InfoBarManager::ShouldShowInfoBar` returns false,
    // in which case this function should do nothing. One case where this can
    // happen is if the --headless command  line switch is present.
    return;
  }

  infobars_[web_contents] = infobar;

  static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())->AddObserver(this);

  auto* content_infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  content_infobar_manager->AddObserver(this);
}

bool DefaultBrowserInfoBarManager::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  return browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
         !browser->GetProfile()->IsIncognitoProfile() &&
         !browser->GetProfile()->IsGuestSession();
}

void DefaultBrowserInfoBarManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      if (!base::Contains(infobars_, contents.contents)) {
        CreateInfoBarForWebContents(contents.contents,
                                    tab_strip_model->profile());
      }
    }
  }
}

void DefaultBrowserInfoBarManager::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                    bool animate) {
  auto infobars_entry = std::ranges::find(
      infobars_, infobar, &decltype(infobars_)::value_type::second);
  if (infobars_entry == infobars_.end()) {
    return;
  }

  infobar->owner()->RemoveObserver(this);
  infobars_.erase(infobars_entry);
  static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())
      ->RemoveObserver(this);

  if (user_initiated_info_bar_close_pending_.has_value()) {
    // Prompt manager will proceed to close all infobars.
    DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
        user_initiated_info_bar_close_pending_.value());
  }
}

void DefaultBrowserInfoBarManager::OnAccept() {
  base::UmaHistogramCounts100("DefaultBrowser.InfoBar.TimesShownBeforeAccept",
                              g_browser_process->local_state()->GetInteger(
                                  prefs::kDefaultBrowserDeclinedCount) +
                                  1);

  user_initiated_info_bar_close_pending_ = CloseReason::kAccept;
}

void DefaultBrowserInfoBarManager::OnDismiss() {
  user_initiated_info_bar_close_pending_ = CloseReason::kDismiss;
}
