// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_BIOMETRIC_AUTHENTICATION_CONFIRMATION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_BIOMETRIC_AUTHENTICATION_CONFIRMATION_BUBBLE_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/biometric_authentication_confirmation_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BiometricAuthenticationConfirmationBubbleView
    : public PasswordBubbleViewBase {
  METADATA_HEADER(BiometricAuthenticationConfirmationBubbleView,
                  PasswordBubbleViewBase)

 public:
  BiometricAuthenticationConfirmationBubbleView(
      content::WebContents* web_contents,
      views::View* anchor_view);
  ~BiometricAuthenticationConfirmationBubbleView() override;

 private:
  // PasswordBubbleViewBase:
  BiometricAuthenticationConfirmationBubbleController* GetController() override;
  const BiometricAuthenticationConfirmationBubbleController* GetController()
      const override;

  // View:
  void AddedToWidget() override;
  void StyledLabelLinkClicked();

  BiometricAuthenticationConfirmationBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_BIOMETRIC_AUTHENTICATION_CONFIRMATION_BUBBLE_VIEW_H_
