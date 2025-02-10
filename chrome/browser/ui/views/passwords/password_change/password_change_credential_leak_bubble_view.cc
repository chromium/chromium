// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_credential_leak_bubble_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_credential_leak_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/vector_icons.h"

using ClosedReason = views::Widget::ClosedReason;

namespace {
// Returns margins for a dialog.
gfx::Insets GetDialogInsets() {
  const auto* const layout_provider = ChromeLayoutProvider::Get();
  gfx::Insets margins = layout_provider->GetInsetsMetric(views::INSETS_DIALOG);
  margins.set_top(0);
  return margins;
}
}  // namespace

PasswordChangeCredentialLeakBubbleView::PasswordChangeCredentialLeakBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  int spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  box_layout->set_cross_axis_alignment(views::LayoutAlignment::kStretch);
  box_layout->SetCollapseMarginsSpacing(true);
  box_layout->set_between_child_spacing(spacing);
  box_layout->set_inside_border_insets(GetDialogInsets());
  // Set the margins to 0 such that the `root_view` fills the whole page bubble
  // width.
  set_margins(gfx::Insets());

  AddChildView(views::Builder<views::StyledLabel>()
                   .SetText(controller_.GetDisplayOrigin())
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetDefaultTextStyle(views::style::STYLE_PRIMARY)
                   .Build());
  AddChildView(CreateBodyText());

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_CHANGE_PASSWORD));
  SetAcceptCallback(base::BindOnce(
      &PasswordChangeCredentialLeakBubbleController::ChangePassword,
      base::Unretained(&controller_)));
  SetCloseCallback(base::BindRepeating(
      [](PasswordChangeCredentialLeakBubbleView* view) {
        ClosedReason reason = view->GetWidget()->closed_reason();
        // Cancel the flow if the dialog is explicitly closed.
        if (reason == ClosedReason::kCloseButtonClicked ||
            reason == ClosedReason::kEscKeyPressed) {
          // `controller_.Cancel()` may trigger Save/Update password prompt, so
          // we need to hide this bubble earlier.
          view->GetWidget()->Hide();
          view->controller_.Cancel();
        }
      },
      this));
}

PasswordChangeCredentialLeakBubbleView::
    ~PasswordChangeCredentialLeakBubbleView() = default;

std::unique_ptr<views::StyledLabel>
PasswordChangeCredentialLeakBubbleView::CreateBodyText() {
  base::RepeatingClosure navigate_to_settings =
      base::BindRepeating(&PasswordChangeCredentialLeakBubbleController::
                              NavigateToPasswordChangeSettings,
                          base::Unretained(&controller_));
  return CreateGooglePasswordManagerLabel(
      /*text_message_id=*/
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_BUBBLE_DETAILS,
      /*link_message_id=*/
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_SETTINGS_LINK,
      controller_.GetPrimaryAccountEmail(), navigate_to_settings,
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY);
}

PasswordBubbleControllerBase*
PasswordChangeCredentialLeakBubbleView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase*
PasswordChangeCredentialLeakBubbleView::GetController() const {
  return &controller_;
}

void PasswordChangeCredentialLeakBubbleView::OnWidgetInitialized() {
  PasswordBubbleViewBase::OnWidgetInitialized();
  GetOkButton()->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(views::kPasswordChangeIcon,
                                     ui::kColorIconSecondary,
                                     GetLayoutConstant(PAGE_INFO_ICON_SIZE)));
  GetOkButton()->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetBubbleHeader(IDR_PASSWORD_CHANGE_WARNING,
                  IDR_PASSWORD_CHANGE_WARNING_DARK);
}

BEGIN_METADATA(PasswordChangeCredentialLeakBubbleView)
END_METADATA
