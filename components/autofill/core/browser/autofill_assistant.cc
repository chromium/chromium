// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_assistant.h"

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

AutofillAssistant::AutofillAssistant(AutofillManager* autofill_manager)
    : credit_card_form_data_(nullptr), autofill_manager_(autofill_manager) {}

AutofillAssistant::~AutofillAssistant() {}

void AutofillAssistant::Reset() {
  credit_card_form_data_.reset();
}

bool AutofillAssistant::CanShowCreditCardAssist() {
  const auto& form_structures = autofill_manager_->form_structures();
  if (form_structures.empty() || credit_card_form_data_ != nullptr ||
      !features::IsAutofillCreditCardAssistEnabled() ||
      // Context of the page is not secure or target URL is valid but not
      // secure.
      !(autofill_manager_->client()->IsContextSecure() &&
        (!form_structures.begin()->second->target_url().is_valid() ||
         !form_structures.begin()->second->target_url().SchemeIs("http")))) {
    return false;
  }

  for (const auto& kv : form_structures) {
    auto& cur_form = kv.second;
    if (cur_form->IsCompleteCreditCardForm()) {
      credit_card_form_data_ =
          std::make_unique<FormData>(cur_form->ToFormData());
      break;
    }
  }
  return credit_card_form_data_ != nullptr;
}

void AutofillAssistant::ShowAssistForCreditCard(const CreditCard& card) {
  DCHECK(credit_card_form_data_);
  autofill_manager_->client()->ConfirmCreditCardFillAssist(
      card, base::BindOnce(&AutofillAssistant::OnUserDidAcceptCreditCardFill,
                           weak_ptr_factory_.GetWeakPtr(), card));
}

void AutofillAssistant::OnUserDidAcceptCreditCardFill(const CreditCard& card) {
  autofill_manager_->GetOrCreateFullCardRequest()->GetFullCard(
      card, AutofillClient::UNMASK_FOR_AUTOFILL, weak_ptr_factory_.GetWeakPtr(),
      autofill_manager_->GetAsFullCardRequestUIDelegate());
}

void AutofillAssistant::OnFullCardRequestSucceeded(
    const payments::FullCardRequest& /* full_card_request */,
    const CreditCard& card,
    const base::string16& cvc) {
  autofill_manager_->FillCreditCardForm(kNoQueryId, *credit_card_form_data_,
                                        credit_card_form_data_->fields[0], card,
                                        cvc);
}

void AutofillAssistant::OnFullCardRequestFailed() {}

}  // namespace autofill
