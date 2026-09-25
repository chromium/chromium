// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
browser_util::PinAppToTaskbarChannel EntrypointToPinToTaskbarChannel(
    default_browser::DefaultBrowserEntrypointType entrypoint_type) {
  switch (entrypoint_type) {
    case default_browser::DefaultBrowserEntrypointType::kBubbleDialog:
      return browser_util::PinAppToTaskbarChannel::kDefaultBrowserBubbleDialog;
    case default_browser::DefaultBrowserEntrypointType::
        kModalDialogWithSettingsIllustration:
    case default_browser::DefaultBrowserEntrypointType::
        kStickyModalDialogWithSettingsIllustration:
      return browser_util::PinAppToTaskbarChannel::
          kDefaultBrowserModalDialogWithSettingsImage;
    case default_browser::DefaultBrowserEntrypointType::
        kModalDialogWithoutSettingsIllustration:
    case default_browser::DefaultBrowserEntrypointType::
        kStickyModalDialogWithoutSettingsIllustration:
      return browser_util::PinAppToTaskbarChannel::
          kDefaultBrowserModalDialogWithoutSettingsImage;
    case default_browser::DefaultBrowserEntrypointType::kStartupInfobar:
      return browser_util::PinAppToTaskbarChannel::kDefaultBrowserInfoBar;
    default:
      NOTREACHED();
  }
}
#endif  // BUILDFLAG(IS_WIN)

bool IsStickyModalEntrypoint(
    default_browser::DefaultBrowserEntrypointType entrypoint_type) {
  return entrypoint_type == default_browser::DefaultBrowserEntrypointType::
                                kStickyModalDialogWithSettingsIllustration ||
         entrypoint_type == default_browser::DefaultBrowserEntrypointType::
                                kStickyModalDialogWithoutSettingsIllustration;
}

}  // namespace

DefaultBrowserSurfaceManager::DefaultBrowserSurfaceManager() = default;

DefaultBrowserSurfaceManager::~DefaultBrowserSurfaceManager() {
  if (controller_) {
    controller_->OnIgnored();
  }
}

void DefaultBrowserSurfaceManager::Show(bool can_pin_to_taskbar) {
  CloseAll();
  can_pin_to_taskbar_ = can_pin_to_taskbar;
  has_accepted_ = false;
  retry_count_.reset();

  controller_ = default_browser::DefaultBrowserManager::CreateControllerFor(
      GetEntrypointType());
  CHECK(controller_);
  controller_->OnShown();

  if (default_browser::IsDefaultBrowserModalSticky()) {
    if (auto* manager =
            default_browser::DefaultBrowserManager::From(g_browser_process)) {
      // Note: `DefaultBrowserMonitor` only runs when
      // `kDefaultBrowserFramework` is enabled.
      default_browser_subscription_ =
          manager->RegisterDefaultBrowserChanged(base::BindRepeating(
              &DefaultBrowserSurfaceManager::OnDefaultBrowserStateChanged,
              base::Unretained(this)));
    }
  }

  auto* global_browser_collection = GlobalBrowserCollection::GetInstance();
  global_browser_collection->ForEach([this](BrowserWindowInterface* bwi) {
    this->OnBrowserCreated(bwi);
    return true;
  });

  browser_collection_observation_.Observe(global_browser_collection);
}

void DefaultBrowserSurfaceManager::CloseAll() {
  RecordRetryCountIfAccepted();
  can_pin_to_taskbar_ = false;
  default_browser_subscription_ = {};
  browser_collection_observation_.Reset();
  CloseAllPromptInstances();
}

bool DefaultBrowserSurfaceManager::IsBrowserValidForShowing(
    BrowserWindowInterface* browser) {
  return browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
         !browser->GetProfile()->IsPrimaryOTRProfileWithRegularParent() &&
         !browser->GetProfile()->IsGuestSession();
}

void DefaultBrowserSurfaceManager::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (!IsBrowserValidForShowing(browser)) {
    return;
  }

  ShowForBrowser(browser);
}

void DefaultBrowserSurfaceManager::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  if (!IsBrowserValidForShowing(browser)) {
    return;
  }

  CloseForBrowser(browser);
}

base::CallbackListSubscription
DefaultBrowserSurfaceManager::RegisterHasAcceptedChanged(
    base::RepeatingCallback<void(bool)> callback) {
  return has_accepted_callbacks_.Add(std::move(callback));
}

void DefaultBrowserSurfaceManager::OnDefaultBrowserStateChanged(
    shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::DefaultWebClientState::IS_DEFAULT) {
    DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kAccept);
  }
}

void DefaultBrowserSurfaceManager::RecordRetryCountIfAccepted() {
  if (!retry_count_) {
    return;
  }
  default_browser::DefaultBrowserController::RecordRetryCount(
      GetEntrypointType(), *retry_count_);
  retry_count_.reset();
}

void DefaultBrowserSurfaceManager::HandleAccept() {
  if (!controller_) {
    return;
  }

  has_accepted_ = true;
  has_accepted_callbacks_.Notify(true);
  if (IsStickyModalEntrypoint(GetEntrypointType())) {
    retry_count_ = 0;
  }

  default_browser::DefaultBrowserSetter::ExecuteParams execute_params;
  if (can_pin_to_taskbar()) {
    execute_params.can_pin_to_taskbar = true;
#if BUILDFLAG(IS_WIN)
    // Attempt the pin to taskbar in parallel with bringing up the Windows
    // settings UI. Serializing the operations is an option, but since the user
    // might not complete the first operation, serializing would probably make
    // the second operation less likely to happen.
    //
    // TODO(crbug.com/343734031): Emit a metric with the pin result. Initially,
    // taskbar_manager.cc metrics will suffice, but taskbar_manager will most
    // likely get used by other code.
    browser_util::PinAppToTaskbar(
        ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
        EntrypointToPinToTaskbarChannel(GetEntrypointType()),
        base::DoNothing());
#else
    NOTREACHED();
#endif  // BUILDFLAG(IS_WIN)
  }

  controller_->OnAccepted(base::DoNothingWithBoundArgs(std::move(controller_)),
                          std::move(execute_params));
}

void DefaultBrowserSurfaceManager::HandleRetry() {
  if (!retry_count_) {
    return;
  }
  // Retries beyond the max are all reported in the "3+" bucket.
  *retry_count_ =
      std::min(*retry_count_ + 1, default_browser::kMaxRecordedRetryCount);
}

void DefaultBrowserSurfaceManager::HandleDismiss() {
  PrefService* local_state = g_browser_process->local_state();

  int declined_count =
      local_state->GetInteger(prefs::kDefaultBrowserDeclinedCount);
  local_state->SetInteger(prefs::kDefaultBrowserDeclinedCount,
                          declined_count + 1);
  local_state->SetTime(prefs::kDefaultBrowserLastDeclinedTime,
                       base::Time::Now());

  if (controller_) {
    controller_->OnDismissed();
    controller_.reset();
  } else if (has_accepted_) {
    default_browser::DefaultBrowserManager::CreateControllerFor(
        GetEntrypointType())
        ->OnDismissed();
  }
}

void DefaultBrowserSurfaceManager::HandleIgnore() {
  if (!controller_) {
    return;
  }

  controller_->OnIgnored();
  controller_.reset();
}

void DefaultBrowserSurfaceManager::OnDialogWidgetCloseRequested(
    BrowserWindowInterface* browser,
    views::Widget::ClosedReason reason) {
  // Note: On Mac, the ESC dismissal is resolved as kUnspecified.
  const bool is_dismiss_action =
      reason == views::Widget::ClosedReason::kEscKeyPressed ||
      reason == views::Widget::ClosedReason::kCloseButtonClicked ||
      reason == views::Widget::ClosedReason::kCancelButtonClicked ||
      reason == views::Widget::ClosedReason::kUnspecified;
  if (is_dismiss_action) {
    HandleDismiss();
    DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kDismiss);
    return;
  }

  RemoveWidget(browser);
}
