// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/failed_password_change_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/failed_password_change_bubble_controller.h"
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

FailedPasswordChangeView::FailedPasswordChangeView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/false),
      controller_(std::make_unique<FailedPasswordChangeBubbleController>(
          PasswordsModelDelegateFromWebContents(web_contents))) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  AddChildView(views::Builder<views::StyledLabel>()
                   .SetText(controller_->GetBody())
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetDefaultTextStyle(views::style::STYLE_PRIMARY)
                   .Build());

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk, controller_->GetAcceptButton());
  SetAcceptCallback(
      base::BindOnce(&FailedPasswordChangeBubbleController::FixManually,
                     base::Unretained(controller_.get())));

  SetFootnoteView(CreateFooterView());

  SetCloseCallback(base::BindRepeating(
      [](FailedPasswordChangeView* view) {
        // When dialog is closed explicitly finish password change flow to
        // transition into a default password manager state.
        if (view->GetWidget()->closed_reason() ==
            views::Widget::ClosedReason::kCloseButtonClicked) {
          view->controller_->FinishPasswordChange();
        }
      },
      this));
}

FailedPasswordChangeView::~FailedPasswordChangeView() = default;

std::unique_ptr<views::View> FailedPasswordChangeView::CreateFooterView() {
  base::RepeatingClosure navigate_to_settings = base::BindRepeating(
      &FailedPasswordChangeBubbleController::NavigateToPasswordChangeSettings,
      base::Unretained(controller_.get()));
  return CreateGooglePasswordManagerLabel(
      /*text_message_id=*/
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FOOTER,
      /*link_message_id=*/
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_SETTINGS_LINK,
      navigate_to_settings);
}

PasswordBubbleControllerBase* FailedPasswordChangeView::GetController() {
  return controller_.get();
}

const PasswordBubbleControllerBase* FailedPasswordChangeView::GetController()
    const {
  return controller_.get();
}

void FailedPasswordChangeView::OnWidgetInitialized() {
  PasswordBubbleViewBase::OnWidgetInitialized();
  GetOkButton()->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                     ui::kColorIconSecondary,
                                     GetLayoutConstant(PAGE_INFO_ICON_SIZE)));
  GetOkButton()->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
}

void FailedPasswordChangeView::AddedToWidget() {
  SetBubbleHeader(IDR_PASSWORD_CHANGE_WARNING,
                  IDR_PASSWORD_CHANGE_WARNING_DARK);
}

BEGIN_METADATA(FailedPasswordChangeView)
END_METADATA
