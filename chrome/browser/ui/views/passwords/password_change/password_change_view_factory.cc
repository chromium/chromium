// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_view_factory.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/views/passwords/password_change/failed_password_change_view.h"
#include "chrome/browser/ui/views/passwords/password_change/no_password_change_form_view.h"
#include "chrome/browser/ui/views/passwords/password_change/otp_during_password_change_view.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_credential_leak_bubble_view.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_info_bubble_view.h"
#include "chrome/browser/ui/views/passwords/password_change/privacy_notice_view.h"
#include "chrome/browser/ui/views/passwords/password_change/successful_password_change_view.h"

PasswordBubbleViewBase* CreatePasswordChangeBubbleView(
    PasswordChangeDelegate* delegate,
    content::WebContents* web_contents,
    views::View* anchor_view) {
  switch (delegate->GetCurrentState()) {
    case PasswordChangeDelegate::State::kOfferingPasswordChange:
      return new PasswordChangeCredentialLeakBubbleView(web_contents,
                                                        anchor_view);
    case PasswordChangeDelegate::State::kWaitingForAgreement:
      return new PrivacyNoticeView(web_contents, anchor_view);
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
    case PasswordChangeDelegate::State::kChangingPassword:
      return new PasswordChangeInfoBubbleView(web_contents, anchor_view,
                                              delegate->GetCurrentState());
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
      return new SuccessfulPasswordChangeView(web_contents, anchor_view);
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
      return new FailedPasswordChangeView(web_contents, anchor_view);
    case PasswordChangeDelegate::State::kChangePasswordFormNotFound:
      return new NoPasswordChangeFormView(web_contents, anchor_view);
    case PasswordChangeDelegate::State::kOtpDetected:
      return new OtpDuringPasswordChangeView(web_contents, anchor_view);
  }
  NOTREACHED();
}
