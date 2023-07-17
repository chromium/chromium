// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/shared_passwords_notification_view.h"

#include "chrome/browser/ui/passwords/bubble_controllers/shared_passwords_notifications_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ui_base_types.h"

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

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_SHARED_PASSWORDS_NOTIFICATION_GOT_IT_BUTTON));
  SetAcceptCallback(base::BindOnce(
      &SharedPasswordsNotificationBubbleController::OnAcknowledgeClicked,
      base::Unretained(&controller_)));

  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_BUTTON));
  SetCancelCallback(base::BindOnce(
      &SharedPasswordsNotificationBubbleController::OnManagePasswordsClicked,
      base::Unretained(&controller_)));
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
