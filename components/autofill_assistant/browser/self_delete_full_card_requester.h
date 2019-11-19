// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELF_DELETE_FULL_CARD_REQUESTER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELF_DELETE_FULL_CARD_REQUESTER_H_

#include <string>

#include "base/callback.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

// Self-deleting requester of full card details, including full PAN and the CVC
// number.
class SelfDeleteFullCardRequester
    : public autofill::payments::FullCardRequest::ResultDelegate {
 public:
  SelfDeleteFullCardRequester();

  void GetFullCard(content::WebContents* web_contents,
                   const autofill::CreditCard* card,
                   ActionDelegate::GetFullCardCallback callback);

 private:
  ~SelfDeleteFullCardRequester() override;

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const autofill::payments::FullCardRequest& /* full_card_request */,
      const autofill::CreditCard& card,
      const base::string16& cvc) override;

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestFailed() override;

  ActionDelegate::GetFullCardCallback callback_;

  base::WeakPtrFactory<SelfDeleteFullCardRequester> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SelfDeleteFullCardRequester);
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELF_DELETE_FULL_CARD_REQUESTER_H_
