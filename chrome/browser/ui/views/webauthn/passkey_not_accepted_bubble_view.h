// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_NOT_ACCEPTED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_NOT_ACCEPTED_BUBBLE_VIEW_H_

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/webauthn/passkey_not_accepted_bubble_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}

namespace ui {
class ImageModel;
}

namespace views {
class View;
}

// A view informing the user that their passkey was deleted because it was not
// present on an AllAcceptedCredentials report.
class PasskeyNotAcceptedBubbleView : public PasswordBubbleViewBase {
  METADATA_HEADER(PasskeyNotAcceptedBubbleView, PasswordBubbleViewBase)

 public:
  PasskeyNotAcceptedBubbleView(content::WebContents* web_contents,
                               views::View* anchor_view,
                               DisplayReason display_reason);
  ~PasskeyNotAcceptedBubbleView() override;
  PasskeyNotAcceptedBubbleView(const PasskeyNotAcceptedBubbleView&) = delete;
  PasskeyNotAcceptedBubbleView& operator=(const PasskeyNotAcceptedBubbleView&) =
      delete;

 private:
  // PasswordBubbleViewBase:
  PasskeyNotAcceptedBubbleController* GetController() override;
  const PasskeyNotAcceptedBubbleController* GetController() const override;
  ui::ImageModel GetWindowIcon() override;

  // Notifies the `controller_` to open password manager and closes the bubble.
  void OnGooglePasswordManagerLinkClicked();

  PasskeyNotAcceptedBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PASSKEY_NOT_ACCEPTED_BUBBLE_VIEW_H_
