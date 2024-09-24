// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_BIOMETRIC_AUTHENTICATION_FOR_FILLING_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_BIOMETRIC_AUTHENTICATION_FOR_FILLING_BUBBLE_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/biometric_authentication_for_filling_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Bubble prompting user to enable biometric authentication before filling
// passwords.
class BiometricAuthenticationForFillingBubbleView
    : public PasswordBubbleViewBase {
  METADATA_HEADER(BiometricAuthenticationForFillingBubbleView,
                  PasswordBubbleViewBase)

 public:
  BiometricAuthenticationForFillingBubbleView(
      content::WebContents* web_contents,
      views::View* anchor_view,
      PrefService* prefs,
      DisplayReason display_reason);
  ~BiometricAuthenticationForFillingBubbleView() override;

 private:
  // PasswordBubbleViewBase:
  BiometricAuthenticationForFillingBubbleController* GetController() override;
  const BiometricAuthenticationForFillingBubbleController* GetController()
      const override;

  // View:
  void AddedToWidget() override;

  BiometricAuthenticationForFillingBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_BIOMETRIC_AUTHENTICATION_FOR_FILLING_BUBBLE_VIEW_H_
