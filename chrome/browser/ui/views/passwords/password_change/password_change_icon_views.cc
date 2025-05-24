// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_icon_views.h"

#include <string>

#include "base/functional/bind.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

using State = PasswordChangeDelegate::State;

namespace {
std::u16string GetLabelText(PasswordChangeDelegate::State state) {
  if (state == PasswordChangeDelegate::State::kWaitingForChangePasswordForm) {
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OMNIBOX_SIGN_IN_CHECK);
  }
  if (state == PasswordChangeDelegate::State::kChangingPassword) {
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_OMNIBOX_CHANGING_PASSWORD);
  }
  return std::u16string();
}
}  // namespace

PasswordChangeIconViews::PasswordChangeIconViews(
    CommandUpdater* updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    Browser* browser)
    : PageActionIconView(updater,
                         IDC_MANAGE_PASSWORDS_FOR_PAGE,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "PasswordChange",
                         kActionShowPasswordsBubbleOrPage,
                         browser),
      controller_(
          base::BindRepeating(&PasswordChangeIconViews::UpdateIconAndLabel,
                              base::Unretained(this)),
          base::BindRepeating(&PasswordChangeIconViews::UpdateImpl,
                              base::Unretained(this))) {
  // Password icon should not be mirrored in RTL.
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(false);
  SetProperty(views::kElementIdentifierKey, kPasswordsOmniboxKeyIconElementId);

  std::u16string tooltip = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_ICON_TOOLTIP);
  SetAccessibleName(tooltip);
  UpdateTooltipText();
  // This doesn't work, the icon color stays the same.
  SetIconColor(ui::kColorSysOnTonalContainer);
}

PasswordChangeIconViews::~PasswordChangeIconViews() = default;

void PasswordChangeIconViews::SetState(password_manager::ui::State state,
                                       bool is_blocklisted) {
  const bool password_change_blocked =
      is_blocklisted &&
      base::FeatureList::IsEnabled(features::kSavePasswordsContextualUi);
  bool should_be_visible =
      !password_change_blocked &&
      state == password_manager::ui::State::PASSWORD_CHANGE_STATE &&
      !delegate()->ShouldHidePageActionIcon(this);
  SetVisible(should_be_visible);
  if (state == password_manager::ui::State::PASSWORD_CHANGE_STATE) {
    std::u16string tooltip = l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_ICON_TOOLTIP);
    SetTooltipForToolbarPinningEnabled(tooltip);
  }

  PasswordChangeDelegate* password_change_delegate =
      GetWebContents() ? PasswordsModelDelegateFromWebContents(GetWebContents())
                             ->GetPasswordChangeDelegate()
                       : nullptr;
  controller_.SetPasswordChangeDelegate(password_change_delegate);

  // We may be about to automatically pop up a passwords bubble.
  // Force layout of the icon's parent now; the bubble will be incorrectly
  // positioned otherwise, as the icon won't have been drawn into position.
  InvalidateLayout();
}

views::BubbleDialogDelegate* PasswordChangeIconViews::GetBubble() const {
  return PasswordBubbleViewBase::manage_password_bubble();
}

void PasswordChangeIconViews::UpdateImpl() {
  if (!GetWebContents()) {
    return;
  }

  ManagePasswordsUIController::FromWebContents(GetWebContents())
      ->UpdateIconAndBubbleState(this);
}

void PasswordChangeIconViews::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  browser()->window()->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHPasswordsSaveRecoveryPromoFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
}

bool PasswordChangeIconViews::OnMousePressed(const ui::MouseEvent& event) {
  bool result = PageActionIconView::OnMousePressed(event);
  PasswordBubbleViewBase::CloseCurrentBubble();
  return result;
}

const gfx::VectorIcon& PasswordChangeIconViews::GetVectorIcon() const {
  switch (controller_.GetCurrentState()) {
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
    case PasswordChangeDelegate::State::kWaitingForAgreement:
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
    case PasswordChangeDelegate::State::kOtpDetected:
      return vector_icons::kPasswordManagerIcon;
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
    case PasswordChangeDelegate::State::kChangingPassword:
      return views::kPasswordChangeIcon;
  }
  return vector_icons::kPasswordManagerIcon;
}

void PasswordChangeIconViews::AboutToRequestFocusFromTabTraversal(
    bool reverse) {
  if (IsBubbleShowing()) {
    PasswordBubbleViewBase::ActivateBubble();
  }
}

void PasswordChangeIconViews::SetTooltipForToolbarPinningEnabled(
    const std::u16string& tooltip) {
  // TODO(crbug.com/353777476): Strip out pinned toolbar button code into a
  // shared controller for page action and pinned button.
  BrowserActions* browser_actions = browser()->browser_actions();
  actions::ActionManager::Get()
      .FindAction(kActionShowPasswordsBubbleOrPage,
                  browser_actions->root_action_item())
      ->SetTooltipText(tooltip);
}

void PasswordChangeIconViews::UpdateIconAndLabel() {
  SkColor icon_color = GetColorProvider()->GetColor(kColorOmniboxActionIcon);
  ui::ColorId background_color_id = GetUseTonalColorsWhenExpanded()
                                        ? kColorOmniboxIconBackgroundTonal
                                        : kColorOmniboxIconBackground;

  State flow_state = controller_.GetCurrentState();
  if (flow_state == State::kWaitingForChangePasswordForm ||
      flow_state == State::kChangingPassword) {
    icon_color = GetColorProvider()->GetColor(ui::kColorSysOnTonalContainer);
    background_color_id = ui::kColorSysTonalContainer;
  }
  std::u16string label_text = GetLabelText(controller_.GetCurrentState());
  SetLabel(label_text,
           /*accessible_name=*/label_text.empty()
               ? l10n_util::GetStringUTF16(
                     IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_ICON_TOOLTIP)
               : label_text);
  label()->SetVisible(label_text.empty() ? false : true);
  SetIconColor(icon_color);
  SetEnabledTextColors(icon_color);
  SetCustomBackgroundColorId(background_color_id);
  UpdateIconImage();
  UpdateLabelColors();
  UpdateBackground();
}

BEGIN_METADATA(PasswordChangeIconViews)
END_METADATA
