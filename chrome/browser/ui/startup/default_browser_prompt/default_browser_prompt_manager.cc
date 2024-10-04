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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_delegate.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace {
bool ShouldShowPrompts() {
  PrefService *local_state = g_browser_process->local_state();

  const int declined_count =
      local_state->GetInteger(prefs::kDefaultBrowserDeclinedCount);
  const base::Time last_declined_time =
      local_state->GetTime(prefs::kDefaultBrowserLastDeclinedTime);
  const int max_prompt_count = features::kMaxPromptCount.Get();

  // A negative value for the max prompt count indicates that the prompt
  // should be shown indefinitely. Otherwise, don't show the prompt if
  // declined count equals or exceeds the max prompt count. A max prompt count
  // of zero should mean that the prompt is never shown.
  if (max_prompt_count >= 0 && declined_count >= max_prompt_count) {
    return false;
  }

  // Show if the user has never declined the prompt.
  if (declined_count == 0) {
    return true;
  }

  // Show if it has been long enough since the last declined time
  base::TimeDelta reprompt_duration =
      features::kRepromptDuration.Get() *
      std::pow(features::kRepromptDurationMultiplier.Get(), declined_count - 1);
  return (base::Time::Now() - last_declined_time) > reprompt_duration;
}
} // namespace

// static
DefaultBrowserPromptManager *DefaultBrowserPromptManager::GetInstance() {
  return base::Singleton<DefaultBrowserPromptManager>::get();
}

void DefaultBrowserPromptManager::AddObserver(Observer *observer) {
  observers_.AddObserver(observer);
}
void DefaultBrowserPromptManager::RemoveObserver(Observer *observer) {
  observers_.RemoveObserver(observer);
}

void DefaultBrowserPromptManager::MaybeShowPrompt() {
  CHECK(base::FeatureList::IsEnabled(features::kDefaultBrowserPromptRefresh));
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  NOTREACHED_IN_MIGRATION()
      << "Unsupported platforms for showing default browser prompts.";
#endif

  if (features::kShowDefaultBrowserAppMenuItem.Get()) {
    SetAppMenuItemVisibility(true);
  }

  if (!ShouldShowPrompts()) {
    return;
  }

  if (features::kShowDefaultBrowserAppMenuChip.Get()) {
    SetShowAppMenuPromptVisibility(true);
  }

  if (features::kShowDefaultBrowserInfoBar.Get()) {
    browser_tab_strip_tracker_ =
        std::make_unique<BrowserTabStripTracker>(this, this);
    browser_tab_strip_tracker_->Init();
  }
}

void DefaultBrowserPromptManager::CloseAllPrompts(CloseReason close_reason) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  NOTREACHED_IN_MIGRATION()
      << "Unsupported platforms for showing default browser prompts.";
#endif

  CloseAllInfoBars();

  SetShowAppMenuPromptVisibility(false);

  if (close_reason == CloseReason::kAccept) {
    SetAppMenuItemVisibility(false);
  }
}

DefaultBrowserPromptManager::DefaultBrowserPromptManager() = default;

DefaultBrowserPromptManager::~DefaultBrowserPromptManager() = default;

void DefaultBrowserPromptManager::CreateInfoBarForWebContents(
    content::WebContents *web_contents, Profile *profile) {
  // Ensure that an infobar hasn't already been created.
  CHECK(!infobars_.contains(web_contents));

  infobars::InfoBar* infobar = DefaultBrowserInfoBarDelegate::Create(
      infobars::ContentInfoBarManager::FromWebContents(web_contents), profile);

  if (infobar == nullptr) {
    // Infobar may be null if `InfoBarManager::ShouldShowInfoBar` returns false,
    // in which case this function should do nothing. One case where this can
    // happen is if the --headless command  line switch is present.
    return;
  }

  infobars_[web_contents] = infobar;

  static_cast<ConfirmInfoBarDelegate *>(infobar->delegate())->AddObserver(this);

  auto *infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobar_manager->AddObserver(this);
}

void DefaultBrowserPromptManager::CloseAllInfoBars() {
  browser_tab_strip_tracker_.reset();

  for (const auto &infobars_entry : infobars_) {
    infobars_entry.second->owner()->RemoveObserver(this);
    infobars_entry.second->RemoveSelf();
  }

  infobars_.clear();
}

void DefaultBrowserPromptManager::SetShowAppMenuPromptVisibility(bool show) {
  if (show == show_app_menu_prompt_) {
    return;
  }

  if (show) {
    PrefService *local_state = g_browser_process->local_state();
    base::TimeDelta app_menu_remaining_duration;
    if (local_state->FindPreference(prefs::kDefaultBrowserFirstShownTime)
            ->IsDefaultValue()) {
      local_state->SetTime(prefs::kDefaultBrowserFirstShownTime,
                           base::Time::Now());
      app_menu_remaining_duration =
          features::kDefaultBrowserAppMenuDuration.Get();
    } else {
      base::Time first_shown_time =
          local_state->GetTime(prefs::kDefaultBrowserFirstShownTime);
      // There is a chance the remaining duration is negative due to time
      // passing since `ShouldShowAppMenuPrompt()` was last checked, so clamp to
      // >= 0.
      app_menu_remaining_duration =
          std::max(features::kDefaultBrowserAppMenuDuration.Get() -
                       (base::Time::Now() - first_shown_time),
                   base::Microseconds(0));
    }

    app_menu_prompt_dismiss_timer_.Start(
        FROM_HERE, app_menu_remaining_duration, base::BindOnce([]() {
          Browser* last_active = BrowserList::GetInstance()->GetLastActive();
          // If there is no active browser, just dismiss the prompts and the
          // prefs will be updated on the next startup.
          if (last_active) {
            chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(
                last_active->profile());
          }
          DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
              CloseReason::kDismiss);
        }));
  } else {
    app_menu_prompt_dismiss_timer_.Stop();
  }

  show_app_menu_prompt_ = show;
  for (auto &obs : observers_) {
    obs.OnShowAppMenuPromptChanged();
  }
}

void DefaultBrowserPromptManager::SetAppMenuItemVisibility(bool show) {
  show_app_menu_item_ = show;
}

bool DefaultBrowserPromptManager::ShouldTrackBrowser(Browser *browser) {
  return browser->is_type_normal() &&
         !browser->profile()->IsIncognitoProfile() &&
         !browser->profile()->IsGuestSession();
}

void DefaultBrowserPromptManager::OnTabStripModelChanged(
    TabStripModel *tab_strip_model, const TabStripModelChange &change,
    const TabStripSelectionChange &selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto &contents : change.GetInsert()->contents) {
      if (!base::Contains(infobars_, contents.contents)) {
        CreateInfoBarForWebContents(contents.contents,
                                    tab_strip_model->profile());
      }
    }
  }
}

void DefaultBrowserPromptManager::OnInfoBarRemoved(infobars::InfoBar *infobar,
                                                   bool animate) {
  auto infobars_entry = base::ranges::find(
      infobars_, infobar, &decltype(infobars_)::value_type::second);
  if (infobars_entry == infobars_.end()) {
    return;
  }

  infobar->owner()->RemoveObserver(this);
  infobars_.erase(infobars_entry);
  static_cast<ConfirmInfoBarDelegate *>(infobar->delegate())
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
