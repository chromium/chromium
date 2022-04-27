// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/full_card_requester.h"

#include <memory>
#include <utility>

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

using autofill::payments::FullCardRequest;

FullCardRequester::FullCardRequester() {}

void FullCardRequester::GetFullCard(
    content::WebContents* web_contents,
    const autofill::CreditCard* card,
    ActionDelegate::GetFullCardCallback callback) {
  DCHECK(card);
  callback_ = std::move(callback);

  autofill::ContentAutofillDriverFactory* factory =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents);
  if (!factory) {
    OnFullCardRequestFailed(FullCardRequest::FailureType::GENERIC_FAILURE);
    return;
  }

  autofill::ContentAutofillDriver* driver =
      factory->DriverForFrame(web_contents->GetMainFrame());
  if (!driver) {
    OnFullCardRequestFailed(FullCardRequest::FailureType::GENERIC_FAILURE);
    return;
  }

  autofill::CreditCardCVCAuthenticator* cvc_authenticator =
      driver->autofill_manager()
          ->GetCreditCardAccessManager()
          ->GetOrCreateCVCAuthenticator();
  cvc_authenticator->GetFullCardRequest()->GetFullCard(
      *card, autofill::AutofillClient::UnmaskCardReason::kAutofill,
      weak_ptr_factory_.GetWeakPtr(),
      cvc_authenticator->GetAsFullCardRequestUIDelegate());
}

FullCardRequester::~FullCardRequester() = default;

void FullCardRequester::OnFullCardRequestSucceeded(
    const FullCardRequest& /* full_card_request */,
    const autofill::CreditCard& card,
    const std::u16string& cvc) {
  std::move(callback_).Run(OkClientStatus(),
                           std::make_unique<autofill::CreditCard>(card), cvc);
}

void FullCardRequester::OnFullCardRequestFailed(
    FullCardRequest::FailureType failure_type) {
  ClientStatus status(GET_FULL_CARD_FAILED);
  AutofillErrorInfoProto::GetFullCardFailureType error_type =
      AutofillErrorInfoProto::UNKNOWN_FAILURE_TYPE;
  switch (failure_type) {
    case FullCardRequest::FailureType::PROMPT_CLOSED:
      error_type = AutofillErrorInfoProto::PROMPT_CLOSED;
      break;
    case FullCardRequest::FailureType::VERIFICATION_DECLINED:
      error_type = AutofillErrorInfoProto::VERIFICATION_DECLINED;
      break;
    case FullCardRequest::FailureType::GENERIC_FAILURE:
      error_type = AutofillErrorInfoProto::GENERIC_FAILURE;
      break;
    default:
      // Adding the default case such that additions to
      // FullCardRequest::FailureType do not need to add new cases to Autofill
      // Assistant code.
      break;
  }
  status.mutable_details()
      ->mutable_autofill_error_info()
      ->set_get_full_card_failure_type(error_type);
  std::move(callback_).Run(status, nullptr, std::u16string());
}
}  // namespace autofill_assistant
