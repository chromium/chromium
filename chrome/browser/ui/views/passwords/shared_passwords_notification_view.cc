// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/shared_passwords_notification_view.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"

SharedPasswordsNotificationView::SharedPasswordsNotificationView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {}

SharedPasswordsNotificationView::~SharedPasswordsNotificationView() = default;

SharedPasswordsNotificationBubbleController*
SharedPasswordsNotificationView::GetController() {
  return &controller_;
}

const SharedPasswordsNotificationBubbleController*
SharedPasswordsNotificationView::GetController() const {
  return &controller_;
}
