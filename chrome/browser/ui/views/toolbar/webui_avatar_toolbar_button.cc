// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_avatar_toolbar_button.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_state_manager.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "ui/base/models/image_model.h"
#include "ui/views/accessibility/view_accessibility.h"

WebUIAvatarToolbarButton::WebUIAvatarToolbarButton(
    WebUIToolbarControlDelegate* delegate,
    Browser* browser)
    : delegate_(delegate) {
  if (browser) {
    state_manager_ =
        std::make_unique<AvatarToolbarButtonStateManager>(*this, browser);
    state_manager_->InitializeStates();
  }
}

WebUIAvatarToolbarButton::~WebUIAvatarToolbarButton() = default;

void WebUIAvatarToolbarButton::Initialize() {
  if (delegate_->GetView()->GetWidget()) {
    CHECK(!is_initialized_);
    is_initialized_ = true;
    UpdateState();
  }
}

void WebUIAvatarToolbarButton::UpdateIcon() {
  if (delegate_->GetView()->GetWidget()) {
    UpdateState();
  }
}

void WebUIAvatarToolbarButton::UpdateText() {
  if (delegate_->GetView()->GetWidget()) {
    UpdateState();
  }
}

void WebUIAvatarToolbarButton::SetAnnounceCallbackForTesting(
    base::OnceCallback<void(std::u16string)> callback) {
  CHECK_IS_TEST();
  announce_callback_for_testing_ = std::move(callback);
}

void WebUIAvatarToolbarButton::AnnounceInternal(std::u16string text) {
  if (announce_callback_for_testing_) {
    std::move(announce_callback_for_testing_).Run(text);
  }
  if (delegate_->GetView()->GetWidget()) {
    delegate_->GetView()->GetViewAccessibility().AnnounceAlert(std::move(text));
  }
}

bool WebUIAvatarToolbarButton::IsMouseHovered() const {
  // TODO(crbug.com/470045174): Implement mouse hover state from WebUI if
  // needed.
  return false;
}

bool WebUIAvatarToolbarButton::HasFocus() const {
  // TODO(crbug.com/470045174): Implement focus state from WebUI if needed.
  return false;
}

views::DialogDelegate* WebUIAvatarToolbarButton::GetDialogDelegate() {
  // TODO(crbug.com/470045174): Implement dialog delegate from WebUI if needed.
  return nullptr;
}

void WebUIAvatarToolbarButton::AddObserver(
    AvatarToolbarButtonInterface::Observer* observer) {
  if (state_manager_) {
    state_manager_->AddObserver(observer);
  }
}

void WebUIAvatarToolbarButton::RemoveObserver(
    AvatarToolbarButtonInterface::Observer* observer) {
  if (state_manager_) {
    state_manager_->RemoveObserver(observer);
  }
}

void WebUIAvatarToolbarButton::ButtonPressed(bool is_source_accelerator) {
  if (state_manager_ && delegate_->GetView()->GetWidget()) {
    state_manager_->HandleButtonPressed(is_source_accelerator);
  }
}

base::ScopedClosureRunner WebUIAvatarToolbarButton::SetExplicitButtonState(
    const std::u16string& text,
    std::optional<std::u16string> accessibility_label,
    std::optional<base::RepeatingCallback<void(bool is_source_accelerator)>>
        explicit_action,
    bool should_announce) {
  if (state_manager_ && delegate_->GetView()->GetWidget()) {
    if (should_announce) {
      // Announce with a delay: if passwords are being uploaded, the OS may be
      // showing a keychain dialog. The keychain dialog is closing and focus is
      // moving back to Chrome. Announcing during this process may result in the
      // announcement to be dropped.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&WebUIAvatarToolbarButton::AnnounceInternal,
                         weak_ptr_factory_.GetWeakPtr(), text),
          AvatarToolbarButtonInterface::kAccessibilityAnnouncementDelay);
    }
    return state_manager_->SetExplicitState(
        text, std::move(accessibility_label), std::move(explicit_action));
  }
  return base::ScopedClosureRunner();
}

bool WebUIAvatarToolbarButton::HasExplicitButtonState() const {
  return state_manager_ && state_manager_->HasExplicitButtonState();
}

void WebUIAvatarToolbarButton::MaybeShowProfileSwitchIPH() {
  if (state_manager_ && delegate_->GetView()->GetWidget()) {
    state_manager_->MaybeShowProfileSwitchIPH();
  }
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void WebUIAvatarToolbarButton::MaybeShowSupervisedUserSignInIPH() {
  if (state_manager_ && delegate_->GetView()->GetWidget()) {
    state_manager_->MaybeShowSupervisedUserSignInIPH();
  }
}

void WebUIAvatarToolbarButton::MaybeShowSignInBenefitsIPH() {
  if (state_manager_ && delegate_->GetView()->GetWidget()) {
    state_manager_->MaybeShowSignInBenefitsIPH();
  }
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

void WebUIAvatarToolbarButton::ClearActiveStateForTesting() {
  CHECK(state_manager_);
  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  CHECK(state_provider);
  state_provider->ClearForTesting();  // IN-TEST
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void WebUIAvatarToolbarButton::ForceShowingPromoForTesting() {
  CHECK(state_manager_);
  state_manager_->ForceShowingPromoForTesting();  // IN-TEST
}

bool WebUIAvatarToolbarButton::
    GetStateAndFireSignedOutTriggerDelayTimerForTesting() {
  CHECK(state_manager_);
  return state_manager_
      ->GetStateAndFireSignedOutTriggerDelayTimerForTesting();  // IN-TEST
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

void WebUIAvatarToolbarButton::NotifyIPHPromoChanged(bool has_promo) {
  if (state_manager_ && delegate_->GetView()->GetWidget()) {
    state_manager_->NotifyIPHPromoChanged(has_promo);
  }
}

void WebUIAvatarToolbarButton::UpdateState() {
  if (!state_manager_ || !is_initialized_ ||
      !delegate_->GetView()->GetWidget()) {
    return;
  }
  // TODO(crbug.com/470045174): Implement Mojo state push once API is added.
  UpdateAccessibilityLabel();
}

void WebUIAvatarToolbarButton::UpdateAccessibilityLabel() {
  CHECK(state_manager_);
  StateProvider* active_state_provider =
      state_manager_->GetActiveStateProvider();
  if (!active_state_provider) {
    return;
  }
  auto [name, description] =
      state_manager_->GetAccessibilityLabels(active_state_provider->GetText());
  accessibility_name_ = std::move(name);
  accessibility_description_ = std::move(description);
}
