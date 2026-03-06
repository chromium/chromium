// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_delegate.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"

using CloseReason = DefaultBrowserPromptManager::CloseReason;

DefaultBrowserInfoBarManager::DefaultBrowserInfoBarManager() = default;
DefaultBrowserInfoBarManager::~DefaultBrowserInfoBarManager() = default;

void DefaultBrowserInfoBarManager::Show(bool can_pin_to_taskbar) {
  DefaultBrowserSurfaceManager::Show(can_pin_to_taskbar);

  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, this);
  // This will trigger a call to `OnTabStripModelChanged`, which will create
  // the info bar.
  browser_tab_strip_tracker_->Init();
}

void DefaultBrowserInfoBarManager::ShowForBrowser(
    BrowserWindowInterface* browser) {
  // The BrowserTabStripTracker will handle showing infobars for both existing
  // and newly created browsers, so we don't need to do anything here.
}

void DefaultBrowserInfoBarManager::CloseForBrowser(
    BrowserWindowInterface* browser) {
  if (user_initiated_info_bar_close_pending_.has_value()) {
    return;
  }

  // If the last browser window that we are tracking is getting closed, and the
  // user hasn't interacted with the infobar yet, we record this as IGNORED.
  bool all_tracked_browser_windows_closed = true;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&all_tracked_browser_windows_closed, this,
       browser](BrowserWindowInterface* bwi) {
        if (bwi != browser && IsBrowserValidForShowing(bwi)) {
          all_tracked_browser_windows_closed = false;
        }
        return all_tracked_browser_windows_closed;
      });

  if (!all_tracked_browser_windows_closed) {
    return;
  }

  // Reset the observers.
  browser_tab_strip_tracker_.reset();

  HandleIgnore();

  base::RecordAction(base::UserMetricsAction("DefaultBrowserInfoBar_Ignore"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            IGNORE_INFO_BAR_PER_SESSION,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);
}

void DefaultBrowserInfoBarManager::CloseAllPromptInstances() {
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
      can_pin_to_taskbar());

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
  return IsBrowserValidForShowing(browser);
}

void DefaultBrowserInfoBarManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      if (!infobars_.contains(contents.contents)) {
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
                                  prefs::kDefaultBrowserInfobarDeclinedCount) +
                                  1);
  base::RecordAction(base::UserMetricsAction("DefaultBrowserInfoBar_Accept"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            ACCEPT_INFO_BAR,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);

  user_initiated_info_bar_close_pending_ = CloseReason::kAccept;

  HandleAccept();
}

void DefaultBrowserInfoBarManager::OnDismiss() {
  HandleDismiss();

  base::RecordAction(base::UserMetricsAction("DefaultBrowserInfoBar_Dismiss"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            DISMISS_INFO_BAR,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);

  user_initiated_info_bar_close_pending_ = CloseReason::kDismiss;
}

default_browser::DefaultBrowserEntrypointType
DefaultBrowserInfoBarManager::GetEntrypointType() const {
  return default_browser::DefaultBrowserEntrypointType::kStartupInfobar;
}
