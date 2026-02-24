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
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_bubble_dialog_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif

namespace {

using default_browser::DefaultBrowserManager;
using default_browser::DefaultBrowserPromptSurface;

bool ShouldShowPrompts() {
  PrefService* local_state = g_browser_process->local_state();

  int declined_count =
      local_state->GetInteger(prefs::kDefaultBrowserInfobarDeclinedCount);
  base::Time last_declined_time =
      local_state->GetTime(prefs::kDefaultBrowserInfobarLastDeclinedTime);

  constexpr int kMaxPromptCount = 5;
  constexpr int kRepromptDurationDays = 21;

  int max_prompt_count = kMaxPromptCount;
  int reprompt_duration_days = kRepromptDurationDays;

  if (default_browser::IsDefaultBrowserPromptSurfacesEnabled()) {
    declined_count =
        local_state->GetInteger(prefs::kDefaultBrowserDeclinedCount);
    last_declined_time =
        local_state->GetTime(prefs::kDefaultBrowserLastDeclinedTime);

    constexpr int kFrameworkMaxPromptCount = 5;
    constexpr int kFrameworkRepromptDurationDays = 14;

    max_prompt_count = kFrameworkMaxPromptCount;
    reprompt_duration_days = kFrameworkRepromptDurationDays;
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kSeparateDefaultAndPinPrompt)) {
    max_prompt_count =
        features::kSeparateDefaultAndPinPromptDefaultMaxCount.Get();
    reprompt_duration_days =
        features::kSeparateDefaultAndPinPromptDefaultCooldownDays.Get();
  }
#endif

  if (declined_count >= max_prompt_count) {
    return false;
  }

  // Show if the user has never declined the prompt.
  if (declined_count == 0) {
    return true;
  }

  // Show if it has been long enough since the last declined time
  return (base::Time::Now() - last_declined_time) >
         base::Days(reprompt_duration_days);
}

DefaultBrowserPromptSurface GetPromptSurface() {
  constexpr int kExperimentSurfaceMaxDeclines = 3;

  PrefService* local_state = g_browser_process->local_state();
  const int decline_count =
      local_state->GetInteger(prefs::kDefaultBrowserDeclinedCount);

  if (decline_count >= kExperimentSurfaceMaxDeclines) {
    return DefaultBrowserPromptSurface::kInfobar;
  }
  return default_browser::GetDefaultBrowserPromptSurface();
}

}  // namespace

// static
DefaultBrowserPromptManager* DefaultBrowserPromptManager::GetInstance() {
  return base::Singleton<DefaultBrowserPromptManager>::get();
}

DefaultBrowserPromptManager::DefaultBrowserPromptManager() = default;

DefaultBrowserPromptManager::~DefaultBrowserPromptManager() = default;

bool DefaultBrowserPromptManager::MaybeShowPrompt() {
  SetAppMenuItemVisibility(true);

  if (!ShouldShowPrompts()) {
    return false;
  }

#if BUILDFLAG(IS_WIN)
  // If the experiment to separate the default browser prompt and the pin to
  // taskbar prompt is enabled, do not offer to pin to taskbar.
  if (base::FeatureList::IsEnabled(features::kSeparateDefaultAndPinPrompt)) {
    ShowPrompts(/*can_pin_to_taskbar=*/false);
    return true;
  }

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
  ShowPrompts(/*can_pin_to_taskbar=*/false);
  return true;
#endif  // BUILDFLAG(IS_WIN)
}

void DefaultBrowserPromptManager::OnCanPinToTaskbarResult(
    bool should_offer_to_pin) {
  ShowPrompts(/*can_pin_to_taskbar=*/should_offer_to_pin);
}

void DefaultBrowserPromptManager::ShowPrompts(bool can_pin_to_taskbar) {
  DefaultBrowserPromptSurface prompt_surface = GetPromptSurface();

  switch (prompt_surface) {
    case DefaultBrowserPromptSurface::kInfobar:
      prompt_surface_manager_ =
          std::make_unique<DefaultBrowserInfoBarManager>();
      break;
    case DefaultBrowserPromptSurface::kBubbleDialog:
      prompt_surface_manager_ =
          std::make_unique<DefaultBrowserBubbleDialogManager>();
      break;
  }
  CHECK(prompt_surface_manager_);

  prompt_surface_manager_->Show(
      default_browser::DefaultBrowserManager::CreateControllerFor(
          prompt_surface_manager_->GetEntrypointType()),
      can_pin_to_taskbar);
}

void DefaultBrowserPromptManager::CloseAllPrompts(CloseReason close_reason) {
  if (prompt_surface_manager_) {
    prompt_surface_manager_->CloseAll();
    prompt_surface_manager_.reset();
  }
  if (close_reason == CloseReason::kAccept) {
    SetAppMenuItemVisibility(false);
  }
}

void DefaultBrowserPromptManager::SetAppMenuItemVisibility(bool show) {
  show_app_menu_item_ = show;
}
