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
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_state_manager.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"

namespace {

toolbar_ui_api::mojom::AvatarToolbarButtonState MapAvatarState(
    ::AvatarToolbarButtonState state) {
  switch (state) {
    case ::AvatarToolbarButtonState::kGuestSession:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::kGuestSession;
    case ::AvatarToolbarButtonState::kIncognitoProfile:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::kIncognitoProfile;
    case ::AvatarToolbarButtonState::kExplicitTextShowing:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::
          kExplicitTextShowing;
    case ::AvatarToolbarButtonState::kOnSignin:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::kOnSignin;
    case ::AvatarToolbarButtonState::kShowIdentityName:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::kShowIdentityName;
    case ::AvatarToolbarButtonState::kSigninPending:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::kSigninPending;
    case ::AvatarToolbarButtonState::kSyncPaused:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::kSyncPaused;
    case ::AvatarToolbarButtonState::kUpgradeClientError:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::
          kUpgradeClientError;
    case ::AvatarToolbarButtonState::kPassphraseError:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::kPassphraseError;
    case ::AvatarToolbarButtonState::kBookmarksLimitExceeded:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::
          kBookmarksLimitExceeded;
    case ::AvatarToolbarButtonState::kSyncError:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::kSyncError;
    case ::AvatarToolbarButtonState::kPasskeysLockedError:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::
          kPasskeysLockedError;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    case ::AvatarToolbarButtonState::kPromo:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::kPromo;
#endif
    case ::AvatarToolbarButtonState::kManagement:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::kManagement;
    case ::AvatarToolbarButtonState::kNormal:
      return toolbar_ui_api::mojom::AvatarToolbarButtonState::kNormal;
  }
  NOTREACHED();
}

}  // namespace

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

void WebUIAvatarToolbarButton::SetAvatarButtonHovered(bool hovered) {
  if (hovered != hovered_) {
    hovered_ = hovered;
    if (state_manager_ && !hovered_) {
      state_manager_->NotifyMouseExited();
    }
    UpdateState();
  }
}

void WebUIAvatarToolbarButton::SetAvatarButtonFocused(bool focused) {
  if (focused != focused_) {
    focused_ = focused;
    if (state_manager_ && !focused_) {
      state_manager_->NotifyBlur();
    }
    UpdateState();
  }
}

void WebUIAvatarToolbarButton::UpdateIcon() {
  if (delegate_->GetView()->GetWidget()) {
    UpdateState();
    if (state_manager_) {
      state_manager_->NotifyIconUpdated();
      state_manager_->NotifyIPHPromoChanged(IsShowingIPHPromo());
    }
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

bool WebUIAvatarToolbarButton::IsShowingIPHPromo() const {
  return is_showing_iph_promo_;
}

bool WebUIAvatarToolbarButton::IsMouseHovered() const {
  return hovered_;
}

bool WebUIAvatarToolbarButton::HasFocus() const {
  return focused_;
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
  if (is_showing_iph_promo_ == has_promo) {
    return;
  }
  is_showing_iph_promo_ = has_promo;
  if (state_manager_ && delegate_->GetView()->GetWidget()) {
    state_manager_->NotifyIPHPromoChanged(has_promo);
  }
}

void WebUIAvatarToolbarButton::UpdateState() {
  if (!state_manager_ || !is_initialized_ ||
      !delegate_->GetView()->GetWidget()) {
    return;
  }

  StateProvider* state_provider = state_manager_->GetActiveStateProvider();
  if (!state_provider) {
    return;
  }

  auto state = toolbar_ui_api::mojom::AvatarControlState::New();

  state->state = MapAvatarState(state_manager_->GetActiveState());

  bool is_enabled = true;
#if BUILDFLAG(IS_CHROMEOS)
  Profile* profile = delegate_->GetBrowser()->GetProfile();
  is_enabled = profile->IsOffTheRecord() && !profile->IsGuestSession() &&
               !profile->GetOTRProfileID().IsCaptivePortal();
#endif

  const ui::ColorProvider* const color_provider =
      delegate_->GetView()->GetColorProvider();
  if (color_provider) {
    int icon_size = GetLayoutConstant(LayoutConstant::kToolbarButtonIconSize);
    bool is_label_present = !state_provider->GetText().empty();
    SkColor icon_color;
    if (is_label_present) {
      std::optional<SkColor> highlight_color =
          state_provider->GetHighlightTextColor(*color_provider);
      icon_color = highlight_color.value_or(color_provider->GetColor(
          kColorAvatarButtonHighlightDefaultForeground));
    } else if (IsShowingIPHPromo()) {
      icon_color = color_provider->GetColor(kColorToolbarFeaturePromoHighlight);
    } else {
      icon_color = color_provider->GetColor(kColorToolbarButtonIcon);
    }

    auto [icon, icon_type] =
        state_provider->GetAvatarIcon(icon_size, icon_color, *color_provider);

    if (!is_enabled) {
      icon = ui::GetDefaultDisabledIconFromImageModel(icon);
    }

    avatar_icon_handle_ = delegate_->GetIconTable().RegisterImageModelTryReuse(
        icon, avatar_icon_handle_);
    state->icon = avatar_icon_handle_;
  } else {
    avatar_icon_handle_ = toolbar_ui_api::IconHandle();
    state->icon = avatar_icon_handle_;
  }

  state->text = state_provider->GetText();
  state->tooltip = state_provider->GetAvatarTooltipText();
  state->has_ai_ring = state_provider->ShouldShowAiAvatarRing();

  auto [name, description] =
      state_manager_->GetAccessibilityLabels(state_provider->GetText());
  state->accessibility_name = name;
  state->accessibility_description = description;
  state->enabled = is_enabled;

  if (delegate_) {
    delegate_->OnAvatarControlStateChanged(std::move(state));
  }
}

void WebUIAvatarToolbarButton::UpdateAccessibilityLabel() {
  UpdateState();
}
