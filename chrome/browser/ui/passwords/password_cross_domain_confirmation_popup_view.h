// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace autofill {
class AutofillPopupViewDelegate;
}

class GURL;

// This interface is used by `PasswordCrossDomainConfirmationPopupController`
// to manage the cross domain password usage confirmation popup. The popup
// view contains a message warning the user that they are about to expose their
// credentials to potentially unrelated web site, giving two options: either
// confirm it or cancel.
class PasswordCrossDomainConfirmationPopupView {
 public:
  virtual ~PasswordCrossDomainConfirmationPopupView() = default;

  // Creates the popup view and shows it. Returns a `WeakPtr` because its
  // lifetime is controlled by its hosting widget.
  // `domain` is the domain of the current web site the popup is triggered on.
  // `password_origin` is the name of the place where the password was
  // originally created, it can be the domain of the web site or the name of
  // the Android application. In general, it should give the user a good
  // understanding of where the password comes from.
  static base::WeakPtr<PasswordCrossDomainConfirmationPopupView> Show(
      base::WeakPtr<autofill::AutofillPopupViewDelegate> delegate,
      const GURL& domain,
      const std::u16string& password_origin,
      base::OnceClosure confirmation_callback,
      base::OnceClosure cancel_callback);

  // Hides and destroy (maybe asynchronously) the view. The `WeakPtr` returned
  // from `Show()` gets invalidated.
  virtual void Hide() = 0;

  // Checks if the popup overlaps with the pictur-in-picture popup.
  virtual bool OverlapsWithPictureInPictureWindow() const = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_VIEW_H_
