// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_POPUP_VIEW_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace url {
class Origin;
}

namespace views {
class Widget;
}

namespace autofill {

class EmailVerificationPopupController;

// Interface for the email verification permission prompt popup.
class EmailVerificationPopupView {
 public:
  EmailVerificationPopupView() = default;
  EmailVerificationPopupView(const EmailVerificationPopupView&) = delete;
  EmailVerificationPopupView& operator=(const EmailVerificationPopupView&) =
      delete;
  EmailVerificationPopupView(EmailVerificationPopupView&&) = delete;
  EmailVerificationPopupView& operator=(EmailVerificationPopupView&&) = delete;
  virtual ~EmailVerificationPopupView() = default;

  // Creates the popup view and shows it. Returns a `WeakPtr` because its
  // lifetime is controlled by its hosting widget.
  static base::WeakPtr<EmailVerificationPopupView> Show(
      base::WeakPtr<EmailVerificationPopupController> controller,
      views::Widget* parent_widget,
      const url::Origin& issuer_origin,
      const std::u16string& email,
      base::OnceCallback<void(bool)> callback);

  // Hides and destroys the popup view.
  virtual void Hide() = 0;

  // Transitions the verify button into its loading state with an active spinner
  // and disables the cancel button.
  virtual void ShowLoadingState() = 0;

  // Checks if the popup overlaps with a picture-in-picture window.
  virtual bool OverlapsWithPictureInPictureWindow() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_POPUP_VIEW_H_
