// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_H_

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"

namespace base {
class TimeDelta;
}

namespace autofill {

class CardUnmaskPromptController {
 public:
  // Interaction.
  virtual void OnUnmaskDialogClosed() = 0;
  virtual void OnUnmaskPromptAccepted(const base::string16& cvc,
                                      const base::string16& exp_month,
                                      const base::string16& exp_year,
                                      bool should_store_pan,
                                      bool enable_fido_auth) = 0;
  virtual void NewCardLinkClicked() = 0;

  // State.
  virtual base::string16 GetWindowTitle() const = 0;
  virtual base::string16 GetInstructionsMessage() const = 0;
  virtual base::string16 GetOkButtonLabel() const = 0;
  virtual int GetCvcImageRid() const = 0;
  virtual bool ShouldRequestExpirationDate() const = 0;
  virtual bool CanStoreLocally() const = 0;
  virtual bool GetStoreLocallyStartState() const = 0;
  virtual bool GetWebauthnOfferStartState() const = 0;
  virtual base::TimeDelta GetSuccessMessageDuration() const = 0;
  virtual AutofillClient::PaymentsRpcResult GetVerificationResult() const = 0;

  // Utilities.
  virtual bool InputCvcIsValid(const base::string16& input_text) const = 0;
  virtual bool InputExpirationIsValid(const base::string16& month,
                                      const base::string16& year) const = 0;
  virtual int GetExpectedCvcLength() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_H_
