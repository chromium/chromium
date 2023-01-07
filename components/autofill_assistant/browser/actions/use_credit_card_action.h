// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_CREDIT_CARD_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_CREDIT_CARD_ACTION_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"

namespace autofill {
class CreditCard;
}  // namespace autofill

namespace autofill_assistant {
class ClientStatus;

// An action to autofill a form using a credit card.
class UseCreditCardAction : public Action {
 public:
  explicit UseCreditCardAction(ActionDelegate* delegate,
                               const ActionProto& proto);

  UseCreditCardAction(const UseCreditCardAction&) = delete;
  UseCreditCardAction& operator=(const UseCreditCardAction&) = delete;

  ~UseCreditCardAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  // Fill the form using data in client memory. Return whether filling succeeded
  // or not through OnFormFilled.
  void FillFormWithData();
  void OnWaitForElement(const ClientStatus& element_status);

  // Called after getting full credit card with its cvc.
  void OnGetFullCard(const ClientStatus& status,
                     std::unique_ptr<autofill::CreditCard> card,
                     const std::u16string& cvc);

  void InitFallbackHandler(const autofill::CreditCard& card,
                           const std::u16string& cvc,
                           bool is_resolved);

  // Called when the form credit card has been filled.
  void ExecuteFallback(const ClientStatus& status);

  // Note: |fallback_handler_| must be a member, because checking for fallbacks
  // is asynchronous and the existence of the handler must be ensured.
  std::unique_ptr<RequiredFieldsFallbackHandler> fallback_handler_;
  std::unique_ptr<autofill::CreditCard> credit_card_;
  Selector selector_;

  ProcessActionCallback process_action_callback_;
  base::WeakPtrFactory<UseCreditCardAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_CREDIT_CARD_ACTION_H_
