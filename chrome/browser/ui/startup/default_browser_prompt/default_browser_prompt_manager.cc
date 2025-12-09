// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif

namespace {

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

}  // namespace

// static
DefaultBrowserPromptManager* DefaultBrowserPromptManager::GetInstance() {
  return base::Singleton<DefaultBrowserPromptManager>::get();
}

DefaultBrowserPromptManager::DefaultBrowserPromptManager()
    : infobar_manager_(std::make_unique<DefaultBrowserInfoBarManager>()) {}

DefaultBrowserPromptManager::~DefaultBrowserPromptManager() = default;

bool DefaultBrowserPromptManager::MaybeShowPrompt() {
  SetAppMenuItemVisibility(true);

  if (!ShouldShowPrompts()) {
    return false;
  }

#if BUILDFLAG(IS_WIN)
  // On Windows, before showing the info bar, determine whether or not to
  // offer to pin to taskbar, and store that result in `this`.
  // base::Unretained is safe because DefaultBrowserInfobarManager is owned by
  // global singleton - DefaultBrowserPromptManager.
  browser_util::ShouldOfferToPin(
      ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
      browser_util::PinAppToTaskbarChannel::kDefaultBrowserInfoBar,
      base::BindOnce(&DefaultBrowserPromptManager::OnCanPinToTaskbarResult,
                     base::Unretained(this)));
  return true;
#else
  infobar_manager_->ShowInfoBars(/*can_pin_to_taskbar=*/false);
  return true;
#endif  // BUILDFLAG(IS_WIN)
}

void DefaultBrowserPromptManager::OnCanPinToTaskbarResult(
    bool should_offer_to_pin) {
  infobar_manager_->ShowInfoBars(/*can_pin_to_taskbar=*/should_offer_to_pin);
}

void DefaultBrowserPromptManager::CloseAllPrompts(CloseReason close_reason) {
  infobar_manager_->CloseAllInfoBars();

  if (close_reason == CloseReason::kAccept) {
    SetAppMenuItemVisibility(false);
  }
}

void DefaultBrowserPromptManager::SetAppMenuItemVisibility(bool show) {
  show_app_menu_item_ = show;
}
