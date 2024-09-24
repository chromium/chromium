// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_UPDATED_CONFIRMATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_UPDATED_CONFIRMATION_VIEW_H_

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/webauthn/passkey_updated_confirmation_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"

// A view informing the user that their passkey was updated.
class PasskeyUpdatedConfirmationView : public PasswordBubbleViewBase {
  METADATA_HEADER(PasskeyUpdatedConfirmationView, PasswordBubbleViewBase)

 public:
  PasskeyUpdatedConfirmationView(content::WebContents* web_contents,
                                 views::View* anchor_view,
                                 DisplayReason display_reason);
  ~PasskeyUpdatedConfirmationView() override;

 private:
  // PasswordBubbleViewBase:
  PasskeyUpdatedConfirmationController* GetController() override;
  const PasskeyUpdatedConfirmationController* GetController() const override;
  ui::ImageModel GetWindowIcon() override;

  // Notifies the `controller_` to open password manager and closes the bubble.
  void OnGooglePasswordManagerLinkClicked();

  PasskeyUpdatedConfirmationController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_UPDATED_CONFIRMATION_VIEW_H_
