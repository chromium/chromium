// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_avatar_toolbar_button.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_state_manager.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "ui/base/models/image_model.h"

WebUIAvatarToolbarButton::WebUIAvatarToolbarButton(
    WebUIToolbarWebView* webui_toolbar_web_view,
    Browser* browser)
    : webui_toolbar_web_view_(webui_toolbar_web_view) {
  if (browser) {
    state_manager_ =
        std::make_unique<AvatarToolbarButtonStateManager>(*this, browser);
    state_manager_->InitializeStates();
  }
}

WebUIAvatarToolbarButton::~WebUIAvatarToolbarButton() = default;

void WebUIAvatarToolbarButton::Initialize() {
  if (webui_toolbar_web_view_->GetWidget()) {
    CHECK(!is_initialized_);
    is_initialized_ = true;
    UpdateState();
  }
}

void WebUIAvatarToolbarButton::UpdateIcon() {
  if (webui_toolbar_web_view_->GetWidget()) {
    UpdateState();
  }
}

void WebUIAvatarToolbarButton::UpdateText() {
  if (webui_toolbar_web_view_->GetWidget()) {
    UpdateState();
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
  if (state_manager_ && webui_toolbar_web_view_->GetWidget()) {
    state_manager_->HandleButtonPressed(is_source_accelerator);
  }
}

base::ScopedClosureRunner WebUIAvatarToolbarButton::SetExplicitButtonState(
    const std::u16string& text,
    std::optional<std::u16string> accessibility_label,
    std::optional<base::RepeatingCallback<void(bool is_source_accelerator)>>
        explicit_action) {
  if (state_manager_ && webui_toolbar_web_view_->GetWidget()) {
    return state_manager_->SetExplicitState(
        text, std::move(accessibility_label), std::move(explicit_action));
  }
  return base::ScopedClosureRunner();
}

bool WebUIAvatarToolbarButton::HasExplicitButtonState() const {
  return state_manager_ && state_manager_->HasExplicitButtonState();
}

void WebUIAvatarToolbarButton::MaybeShowProfileSwitchIPH() {
  if (state_manager_ && webui_toolbar_web_view_->GetWidget()) {
    state_manager_->MaybeShowProfileSwitchIPH();
  }
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void WebUIAvatarToolbarButton::MaybeShowSupervisedUserSignInIPH() {
  if (state_manager_ && webui_toolbar_web_view_->GetWidget()) {
    state_manager_->MaybeShowSupervisedUserSignInIPH();
  }
}

void WebUIAvatarToolbarButton::MaybeShowSignInBenefitsIPH() {
  if (state_manager_ && webui_toolbar_web_view_->GetWidget()) {
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
  if (state_manager_ && webui_toolbar_web_view_->GetWidget()) {
    state_manager_->NotifyIPHPromoChanged(has_promo);
  }
}

void WebUIAvatarToolbarButton::UpdateState() {
  if (!state_manager_ || !is_initialized_ ||
      !webui_toolbar_web_view_->GetWidget()) {
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
