// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ASSISTANT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ASSISTANT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class AutofillManager;
class CreditCard;

// This class encompasses the triggering rules and the logic for the autofill
// assisted filling mechanisms.
class AutofillAssistant : public payments::FullCardRequest::ResultDelegate {
 public:
  explicit AutofillAssistant(AutofillManager* autofill_manager);
  ~AutofillAssistant() override;

  // Should be called at every page navigation to clear state.
  void Reset();

  // Returns whether a credit card assist can be shown. Will go through the
  // forms in autofill_manager_.form_structures() and extract the credit card
  // form.
  bool CanShowCreditCardAssist();

  // Will show an assist infobar for the previously extracted form proposing to
  // autofill it. Should only be called if CanShowCreditCardAssist() returned
  // true.
  void ShowAssistForCreditCard(const CreditCard& card);

 private:
  // Called by the infobar delegate when the user accepts the infobar.
  void OnUserDidAcceptCreditCardFill(const CreditCard& card);

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const payments::FullCardRequest& full_card_request,
      const CreditCard& card,
      const base::string16& cvc) override;
  void OnFullCardRequestFailed() override;

  // Holds the FormData to be filled with a credit card.
  std::unique_ptr<FormData> credit_card_form_data_;

  // Weak reference to the AutofillManager that created and maintains this
  // AutofillAssistant.
  AutofillManager* autofill_manager_;

  base::WeakPtrFactory<AutofillAssistant> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AutofillAssistant);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ASSISTANT_H_
