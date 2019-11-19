// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/self_delete_full_card_requester.h"

#include <memory>
#include <utility>

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

SelfDeleteFullCardRequester::SelfDeleteFullCardRequester() {}

void SelfDeleteFullCardRequester::GetFullCard(
    content::WebContents* web_contents,
    const autofill::CreditCard* card,
    ActionDelegate::GetFullCardCallback callback) {
  DCHECK(card);
  callback_ = std::move(callback);

  autofill::ContentAutofillDriverFactory* factory =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents);
  if (!factory) {
    OnFullCardRequestFailed();
    return;
  }

  autofill::ContentAutofillDriver* driver =
      factory->DriverForFrame(web_contents->GetMainFrame());
  if (!driver) {
    OnFullCardRequestFailed();
    return;
  }

  driver->autofill_manager()->GetOrCreateFullCardRequest()->GetFullCard(
      *card, autofill::AutofillClient::UNMASK_FOR_AUTOFILL,
      weak_ptr_factory_.GetWeakPtr(),
      driver->autofill_manager()->GetAsFullCardRequestUIDelegate());
}

SelfDeleteFullCardRequester::~SelfDeleteFullCardRequester() = default;

void SelfDeleteFullCardRequester::OnFullCardRequestSucceeded(
    const autofill::payments::FullCardRequest& /* full_card_request */,
    const autofill::CreditCard& card,
    const base::string16& cvc) {
  std::move(callback_).Run(std::make_unique<autofill::CreditCard>(card), cvc);
  delete this;
}

void SelfDeleteFullCardRequester::OnFullCardRequestFailed() {
  // Failed might because of cancel, so return nullptr to notice caller.
  //
  // TODO(crbug.com/806868): Split the fail notification so that "cancel" and
  // "wrong cvc" states can be handled differently. One should prompt a retry,
  // the other a graceful shutdown - the current behavior.
  std::move(callback_).Run(nullptr, base::string16());
  delete this;
}
}  // namespace autofill_assistant
