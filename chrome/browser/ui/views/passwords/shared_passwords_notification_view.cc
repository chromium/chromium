// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/shared_passwords_notification_view.h"

#include <memory>

#include "chrome/browser/ui/passwords/bubble_controllers/shared_passwords_notifications_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

SharedPasswordsNotificationView::SharedPasswordsNotificationView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetShowIcon(true);

  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_SHARED_PASSWORDS_NOTIFICATION_GOT_IT_BUTTON));
  SetAcceptCallback(base::BindOnce(
      &SharedPasswordsNotificationBubbleController::OnAcknowledgeClicked,
      base::Unretained(&controller_)));

  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_BUTTON));
  SetCancelCallback(base::BindOnce(
      &SharedPasswordsNotificationBubbleController::OnManagePasswordsClicked,
      base::Unretained(&controller_)));
  SetCloseCallback(base::BindOnce(
      &SharedPasswordsNotificationBubbleController::OnCloseBubbleClicked,
      base::Unretained(&controller_)));

  SetLayoutManager(std::make_unique<views::BoxLayout>());

  views::StyledLabel* styled_label =
      AddChildView(views::Builder<views::StyledLabel>()
                       .SetText(controller_.GetNotificationBody())
                       .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                       .Build());

  gfx::Range sender_name_range = controller_.GetSenderNameRange();
  if (!sender_name_range.is_empty()) {
    views::StyledLabel::RangeStyleInfo bold_style;
    bold_style.custom_font = styled_label->GetFontList().Derive(
        /*size_delta=*/0, gfx::Font::FontStyle::NORMAL,
        gfx::Font::Weight::BOLD);
    styled_label->AddStyleRange(sender_name_range, bold_style);
  }

  SetProperty(views::kElementIdentifierKey, kTopView);
}

SharedPasswordsNotificationView::~SharedPasswordsNotificationView() = default;

SharedPasswordsNotificationBubbleController*
SharedPasswordsNotificationView::GetController() {
  return &controller_;
}

const SharedPasswordsNotificationBubbleController*
SharedPasswordsNotificationView::GetController() const {
  return &controller_;
}

ui::ImageModel SharedPasswordsNotificationView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

BEGIN_METADATA(SharedPasswordsNotificationView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SharedPasswordsNotificationView,
                                      kTopView);
