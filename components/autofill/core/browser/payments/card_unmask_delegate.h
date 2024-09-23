// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_DELEGATE_H_

#include <string>


namespace autofill {

class CardUnmaskDelegate {
 public:
  struct UserProvidedUnmaskDetails {
    UserProvidedUnmaskDetails();
    UserProvidedUnmaskDetails(const UserProvidedUnmaskDetails& other);
    ~UserProvidedUnmaskDetails();

    // User input data.
    std::u16string cvc;

    // Two digit month.
    std::u16string exp_month;

    // Four digit year.
    std::u16string exp_year;

    // User is opting-in for FIDO Authentication for future card unmasking.
    bool enable_fido_auth = false;

    // If the FIDO auth checkbox was visible to the user.
    bool was_checkbox_visible = false;
  };

  // Called when the user has attempted a verification. Prompt is still
  // open at this point.
  virtual void OnUnmaskPromptAccepted(
      const UserProvidedUnmaskDetails& details) = 0;

  // Called when the unmask prompt is cancelled. This specifically refers to the
  // flow being aborted, and is not invoked when the prompt is closed after card
  // unmask flow is finished successfully.
  virtual void OnUnmaskPromptCancelled() = 0;

  // Returns whether or not the user, while on the CVC prompt, should be
  // offered to switch to FIDO authentication for card unmasking. This will
  // always be false for Desktop since FIDO authentication is offered as a
  // separate prompt after the CVC prompt. On Android, however, this is offered
  // through a checkbox on the CVC prompt. This feature does not yet exist on
  // iOS.
  virtual bool ShouldOfferFidoAuth() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CARD_UNMASK_DELEGATE_H_
