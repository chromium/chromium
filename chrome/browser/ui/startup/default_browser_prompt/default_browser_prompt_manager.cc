// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_delegate.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
bool ShouldShowPrompts() {
  PrefService* local_state = g_browser_process->local_state();

  const int declined_count =
      local_state->GetInteger(prefs::kDefaultBrowserDeclinedCount);
  const base::Time last_declined_time =
      local_state->GetTime(prefs::kDefaultBrowserLastDeclinedTime);
  constexpr int kMaxPromptCount = 5;
  constexpr int kRepromptDurationDays = 21;

  // A negative value for the max prompt count indicates that the prompt
  // should be shown indefinitely. Otherwise, don't show the prompt if
  // declined count equals or exceeds the max prompt count. A max prompt count
  // of zero should mean that the prompt is never shown.
  if (declined_count >= kMaxPromptCount) {
    return false;
  }

  // Show if the user has never declined the prompt.
  if (declined_count == 0) {
    return true;
  }

  // Show if it has been long enough since the last declined time
  return (base::Time::Now() - last_declined_time) >
         base::Days(kRepromptDurationDays);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
}  // namespace

// static
DefaultBrowserPromptManager* DefaultBrowserPromptManager::GetInstance() {
  return base::Singleton<DefaultBrowserPromptManager>::get();
}

void DefaultBrowserPromptManager::InitTabStripTracker() {
  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, this);
  // This will trigger a call to `OnTabStripModelChanged`, which will create
  // the info bar.
  browser_tab_strip_tracker_->Init();
}

bool DefaultBrowserPromptManager::MaybeShowPrompt() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Unsupported platforms for showing default browser prompts.";
#else
  SetAppMenuItemVisibility(true);

  if (!ShouldShowPrompts()) {
    return false;
  }

#if BUILDFLAG(IS_WIN)
  // On Windows, before showing the info bar, determine whether or not to
  // offer to pin to taskbar, and store that result in `this`.
  if (base::FeatureList::IsEnabled(
          features::kOfferPinToTaskbarWhenSettingToDefault)) {
    // base::Unretained is safe because DefaultBrowserPromptManager is a
    // global singleton.
    browser_util::ShouldOfferToPin(
        ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
        browser_util::PinAppToTaskbarChannel::kDefaultBrowserInfoBar,
        base::BindOnce(&DefaultBrowserPromptManager::OnCanPinToTaskbarResult,
                       base::Unretained(this)));
    return true;
  }
#endif  // BUILDFLAG(IS_WIN)

  InitTabStripTracker();
  return true;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
}

void DefaultBrowserPromptManager::OnCanPinToTaskbarResult(
    bool should_offer_to_pin) {
  can_pin_to_taskbar_ = should_offer_to_pin;
  InitTabStripTracker();
}

void DefaultBrowserPromptManager::CloseAllPrompts(CloseReason close_reason) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Unsupported platforms for showing default browser prompts.";
#else
  CloseAllInfoBars();

  if (close_reason == CloseReason::kAccept) {
    SetAppMenuItemVisibility(false);
  }
#endif
}

DefaultBrowserPromptManager::DefaultBrowserPromptManager() = default;

DefaultBrowserPromptManager::~DefaultBrowserPromptManager() = default;

void DefaultBrowserPromptManager::CreateInfoBarForWebContents(
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

  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobar_manager->AddObserver(this);
}

void DefaultBrowserPromptManager::CloseAllInfoBars() {
  browser_tab_strip_tracker_.reset();

  for (const auto& infobars_entry : infobars_) {
    infobars_entry.second->owner()->RemoveObserver(this);
    infobars_entry.second->RemoveSelf();
  }

  infobars_.clear();
}

void DefaultBrowserPromptManager::SetAppMenuItemVisibility(bool show) {
  show_app_menu_item_ = show;
}

bool DefaultBrowserPromptManager::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  return browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
         !browser->GetProfile()->IsIncognitoProfile() &&
         !browser->GetProfile()->IsGuestSession();
}

void DefaultBrowserPromptManager::OnTabStripModelChanged(
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

void DefaultBrowserPromptManager::OnInfoBarRemoved(infobars::InfoBar* infobar,
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
    CloseAllPrompts(user_initiated_info_bar_close_pending_.value());
    user_initiated_info_bar_close_pending_.reset();
  }
}

void DefaultBrowserPromptManager::OnAccept() {
  base::UmaHistogramCounts100("DefaultBrowser.InfoBar.TimesShownBeforeAccept",
                              g_browser_process->local_state()->GetInteger(
                                  prefs::kDefaultBrowserDeclinedCount) +
                                  1);
  user_initiated_info_bar_close_pending_ = CloseReason::kAccept;
}

void DefaultBrowserPromptManager::OnDismiss() {
  user_initiated_info_bar_close_pending_ = CloseReason::kDismiss;
}
