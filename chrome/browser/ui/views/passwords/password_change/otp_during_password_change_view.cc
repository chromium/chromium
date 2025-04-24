// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/otp_during_password_change_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"

OtpDuringPasswordChangeView::OtpDuringPasswordChangeView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  AddChildView(views::Builder<views::StyledLabel>()
                   .SetText(controller_.GetBody())
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetDefaultTextStyle(views::style::STYLE_PRIMARY)
                   .Build());

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller_.GetAcceptButtonText());
  SetAcceptCallback(
      base::BindOnce(&OtpDuringPasswordChangeBubbleController::FixManually,
                     base::Unretained(&controller_)));

  SetFootnoteView(CreateFooterView());
  SetCloseCallback(base::BindOnce(&OtpDuringPasswordChangeView::OnViewClosed,
                                  base::Unretained(this)));
}

OtpDuringPasswordChangeView::~OtpDuringPasswordChangeView() = default;

PasswordBubbleControllerBase* OtpDuringPasswordChangeView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* OtpDuringPasswordChangeView::GetController()
    const {
  return &controller_;
}

void OtpDuringPasswordChangeView::OnWidgetInitialized() {
  PasswordBubbleViewBase::OnWidgetInitialized();
  GetOkButton()->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                     ui::kColorIconSecondary,
                                     GetLayoutConstant(PAGE_INFO_ICON_SIZE)));
  GetOkButton()->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
}

void OtpDuringPasswordChangeView::AddedToWidget() {
  SetBubbleHeader(IDR_PASSWORD_CHANGE_NEUTRAL,
                  IDR_PASSWORD_CHANGE_NEUTRAL_DARK);
}

std::unique_ptr<views::View> OtpDuringPasswordChangeView::CreateFooterView() {
  base::RepeatingClosure navigate_to_settings = base::BindRepeating(
      &OtpDuringPasswordChangeBubbleController::
          NavigateToPasswordChangeSettings,
      // Unretained is safe because this always outlives the footer.
      base::Unretained(&controller_));
  return CreateGooglePasswordManagerLabel(
      /*text_message_id=*/
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FOOTER,
      /*link_message_id=*/
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_SETTINGS_LINK,
      navigate_to_settings);
}

void OtpDuringPasswordChangeView::OnViewClosed() {
  // When dialog is closed explicitly finish password change flow to
  // transition into a default password manager state.
  if (GetWidget()->closed_reason() ==
      views::Widget::ClosedReason::kCloseButtonClicked) {
    controller_.FinishPasswordChange();
  }
}

BEGIN_METADATA(OtpDuringPasswordChangeView)
END_METADATA
