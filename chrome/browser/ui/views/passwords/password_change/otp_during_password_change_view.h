// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_OTP_DURING_PASSWORD_CHANGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_OTP_DURING_PASSWORD_CHANGE_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/otp_during_password_change_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

class OtpDuringPasswordChangeView : public PasswordBubbleViewBase {
  METADATA_HEADER(OtpDuringPasswordChangeView, PasswordBubbleViewBase)

 public:
  OtpDuringPasswordChangeView(content::WebContents* web_contents,
                              views::View* anchor_view);

  // PasswordBubbleViewBase:
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // View:
  void AddedToWidget() override;
  void OnWidgetInitialized() override;

 private:
  ~OtpDuringPasswordChangeView() override;

  std::unique_ptr<views::View> CreateFooterView();
  void OnViewClosed();

  OtpDuringPasswordChangeBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_OTP_DURING_PASSWORD_CHANGE_VIEW_H_
