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

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
void PinToTaskbarResult(bool pinned) {
  // TODO(crbug.com/343734031): Emit a metric with the pin result. Initially,
  // taskbar_manager.cc metrics will suffice, but taskbar_manager will most
  // likely get used by other code.
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

using CloseReason = DefaultBrowserPromptManager::CloseReason;

DefaultBrowserInfoBarManager::DefaultBrowserInfoBarManager() = default;
DefaultBrowserInfoBarManager::~DefaultBrowserInfoBarManager() = default;

void DefaultBrowserInfoBarManager::Show(
    std::unique_ptr<default_browser::DefaultBrowserController> controller,
    bool can_pin_to_taskbar) {
  can_pin_to_taskbar_ = can_pin_to_taskbar;

  default_browser_controller_ = std::move(controller);
  default_browser_controller_->OnShown();

  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, this);
  // This will trigger a call to `OnTabStripModelChanged`, which will create
  // the info bar.
  browser_tab_strip_tracker_->Init();
}

void DefaultBrowserInfoBarManager::CloseAll() {
  can_pin_to_taskbar_ = false;
  user_initiated_info_bar_close_pending_.reset();

  browser_collection_observation_.Reset();
  browser_tab_strip_tracker_.reset();

  for (const auto& infobars_entry : infobars_) {
    infobars_entry.second->owner()->RemoveObserver(this);
    infobars_entry.second->RemoveSelf();
  }

  infobars_.clear();
}

void DefaultBrowserInfoBarManager::OnBrowserClosed(
    BrowserWindowInterface* /*browser*/) {
  if (user_initiated_info_bar_close_pending_.has_value()) {
    return;
  }

  // If the last browser window that we are tracking is getting closed, and the
  // user hasn't interacted with the infobar yet, we record this as IGNORED.
  bool all_tracked_browser_windows_closed = true;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&all_tracked_browser_windows_closed,
       this](BrowserWindowInterface* browser) {
        if (ShouldTrackBrowser(browser)) {
          all_tracked_browser_windows_closed = false;
        }
        return all_tracked_browser_windows_closed;
      });

  if (!all_tracked_browser_windows_closed) {
    return;
  }

  // Reset the observers.
  browser_tab_strip_tracker_.reset();
  browser_collection_observation_.Reset();

  default_browser_controller_->OnIgnored();
  default_browser_controller_.reset();

  base::RecordAction(base::UserMetricsAction("DefaultBrowserInfoBar_Ignore"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            IGNORE_INFO_BAR_PER_SESSION,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);
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

  // The controller will be destroyed once the callback is executed.
  default_browser_controller_->OnAccepted(
      base::DoNothingWithBoundArgs(std::move(default_browser_controller_)));

  if (can_pin_to_taskbar_) {
#if BUILDFLAG(IS_WIN)
    // Attempt the pin to taskbar in parallel with bringing up the Windows
    // settings UI. Serializing the operations is an option, but since the user
    // might not complete the first operation, serializing would probably make
    // the second operation less likely to happen.
    browser_util::PinAppToTaskbar(
        ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
        browser_util::PinAppToTaskbarChannel::kDefaultBrowserInfoBar,
        base::BindOnce(&PinToTaskbarResult));
#else
    NOTREACHED();
#endif  // BUILDFLAG(IS_WIN)
  }
}

void DefaultBrowserInfoBarManager::OnDismiss() {
  default_browser_controller_->OnDismissed();
  default_browser_controller_.reset();

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
