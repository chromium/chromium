// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FULL_CARD_REQUESTER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FULL_CARD_REQUESTER_H_

#include <string>

#include "base/callback.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

// Requester of full card details, including full PAN and the CVC number.
class FullCardRequester
    : public autofill::payments::FullCardRequest::ResultDelegate {
 public:
  FullCardRequester();
  ~FullCardRequester() override;

  FullCardRequester(const FullCardRequester&) = delete;
  FullCardRequester& operator=(const FullCardRequester&) = delete;

  void GetFullCard(content::WebContents* web_contents,
                   const autofill::CreditCard* card,
                   ActionDelegate::GetFullCardCallback callback);

 private:
  // autofill::payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const autofill::payments::FullCardRequest& /* full_card_request */,
      const autofill::CreditCard& card,
      const std::u16string& cvc) override;
  void OnFullCardRequestFailed(
      autofill::payments::FullCardRequest::FailureType /* failure_type */)
      override;

  ActionDelegate::GetFullCardCallback callback_;

  base::WeakPtrFactory<FullCardRequester> weak_ptr_factory_{this};
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FULL_CARD_REQUESTER_H_
