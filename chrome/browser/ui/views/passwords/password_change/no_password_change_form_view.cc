// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/no_password_change_form_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_change/no_password_change_form_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"

NoPasswordChangeFormView::NoPasswordChangeFormView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/false),
      controller_(std::make_unique<NoPasswordChangeFormBubbleController>(
          PasswordsModelDelegateFromWebContents(web_contents))) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  AddChildView(views::Builder<views::StyledLabel>()
                   .SetText(controller_->GetBody())
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetDefaultTextStyle(views::style::STYLE_PRIMARY)
                   .Build());

  SetButtons(
      static_cast<int>(static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel)));
  SetButtonLabel(ui::mojom::DialogButton::kOk, controller_->GetAcceptButton());

  SetAcceptCallback(
      base::BindOnce(&NoPasswordChangeFormBubbleController::Restart,
                     base::Unretained(controller_.get())));
  SetCancelCallback(
      base::BindOnce(&NoPasswordChangeFormBubbleController::Cancel,
                     base::Unretained(controller_.get())));
}

NoPasswordChangeFormView::~NoPasswordChangeFormView() = default;

PasswordBubbleControllerBase* NoPasswordChangeFormView::GetController() {
  return controller_.get();
}

const PasswordBubbleControllerBase* NoPasswordChangeFormView::GetController()
    const {
  return controller_.get();
}

void NoPasswordChangeFormView::OnWidgetInitialized() {
  PasswordBubbleViewBase::OnWidgetInitialized();
  GetOkButton()->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kReloadChromeRefreshIcon,
                                     ui::kColorIconSecondary,
                                     GetLayoutConstant(PAGE_INFO_ICON_SIZE)));
  SetBubbleHeader(IDR_PASSWORD_CHANGE_NEUTRAL,
                  IDR_PASSWORD_CHANGE_NEUTRAL_DARK);
}

BEGIN_METADATA(NoPasswordChangeFormView)
END_METADATA
