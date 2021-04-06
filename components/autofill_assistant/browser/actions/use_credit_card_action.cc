// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_credit_card_action.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {

UseCreditCardAction::UseCreditCardAction(ActionDelegate* delegate,
                                         const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_use_card());
  selector_ = Selector(proto.use_card().form_field_element());
}

UseCreditCardAction::~UseCreditCardAction() = default;

void UseCreditCardAction::InternalProcessAction(
    ProcessActionCallback action_callback) {
  process_action_callback_ = std::move(action_callback);

  if (selector_.empty() && !proto_.use_card().skip_autofill()) {
    VLOG(1) << "UseCreditCard failed: |selector| empty";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }
  if (proto_.use_card().skip_autofill() &&
      proto_.use_card().required_fields().empty()) {
    VLOG(1) << "UseCreditCard failed: |skip_autofill| without required fields";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  // Ensure data already selected in a previous action.
  if (proto_.use_card().has_model_identifier()) {
    if (proto_.use_card().model_identifier().empty()) {
      VLOG(1) << "UseCreditCard failed: |model_identifier| set but empty";
      EndAction(ClientStatus(INVALID_ACTION));
      return;
    }
    auto credit_card_value = delegate_->GetUserModel()->GetValue(
        proto_.use_card().model_identifier());
    if (!credit_card_value.has_value()) {
      VLOG(1) << "UseCreditCard failed: "
              << proto_.use_card().model_identifier()
              << " not found in user model";
      EndAction(ClientStatus(PRECONDITION_FAILED));
      return;
    }
    if (credit_card_value->credit_cards().values().size() != 1) {
      VLOG(1) << "UseCreditCard failed: expected single card for "
              << proto_.use_card().model_identifier() << ", but got "
              << *credit_card_value;
    }
    auto* credit_card = delegate_->GetUserModel()->GetCreditCard(
        credit_card_value->credit_cards().values(0).guid());
    if (credit_card == nullptr) {
      VLOG(1) << "UseCreditCard failed: card not found for guid "
              << *credit_card_value;
      EndAction(ClientStatus(PRECONDITION_FAILED));
      return;
    }
    credit_card_ = std::make_unique<autofill::CreditCard>(*credit_card);
  } else {
    auto* credit_card = delegate_->GetUserData()->selected_card_.get();
    if (credit_card == nullptr) {
      VLOG(1) << "UseCreditCard failed: card not found in user_data";
      EndAction(ClientStatus(PRECONDITION_FAILED));
      return;
    }
    credit_card_ = std::make_unique<autofill::CreditCard>(*credit_card);
  }
  DCHECK(credit_card_ != nullptr);

  FillFormWithData();
}

void UseCreditCardAction::EndAction(const ClientStatus& status) {
  if (fallback_handler_)
    action_stopwatch_.TransferToWaitTime(fallback_handler_->TotalWaitTime());

  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

void UseCreditCardAction::FillFormWithData() {
  if (selector_.empty()) {
    DCHECK(proto_.use_card().skip_autofill());
    OnWaitForElement(OkClientStatus());
    return;
  }

  delegate_->ShortWaitForElementWithSlowWarning(
      selector_,
      base::BindOnce(&UseCreditCardAction::OnWaitForElementTimed,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&UseCreditCardAction::OnWaitForElement,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void UseCreditCardAction::OnWaitForElement(const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  DCHECK(credit_card_ != nullptr);
  delegate_->GetFullCard(credit_card_.get(),
                         base::BindOnce(&UseCreditCardAction::OnGetFullCard,
                                        weak_ptr_factory_.GetWeakPtr()));
  action_stopwatch_.StartWaitTime();
}

void UseCreditCardAction::OnGetFullCard(
    const ClientStatus& status,
    std::unique_ptr<autofill::CreditCard> card,
    const std::u16string& cvc) {
  action_stopwatch_.StartActiveTime();
  if (!status.ok()) {
    EndAction(status);
    return;
  }
  DCHECK(card);

  std::vector<RequiredField> required_fields;
  for (const auto& required_field_proto : proto_.use_card().required_fields()) {
    if (!required_field_proto.has_value_expression()) {
      continue;
    }

    RequiredField required_field;
    required_field.FromProto(required_field_proto);
    required_fields.emplace_back(required_field);
  }

  std::map<std::string, std::string> fallback_values =
      field_formatter::CreateAutofillMappings(*card,
                                              /* locale = */ "en-US");
  fallback_values.emplace(
      base::NumberToString(
          static_cast<int>(AutofillFormatProto::CREDIT_CARD_VERIFICATION_CODE)),
      base::UTF16ToUTF8(cvc));
  fallback_values.emplace(
      base::NumberToString(
          static_cast<int>(AutofillFormatProto::CREDIT_CARD_RAW_NUMBER)),
      base::UTF16ToUTF8(card->GetRawInfo(autofill::CREDIT_CARD_NUMBER)));

  DCHECK(fallback_handler_ == nullptr);
  fallback_handler_ = std::make_unique<RequiredFieldsFallbackHandler>(
      required_fields, fallback_values, delegate_);

  if (proto_.use_card().skip_autofill()) {
    ExecuteFallback(OkClientStatus());
    return;
  }

  DCHECK(!selector_.empty());
  delegate_->FillCardForm(std::move(card), cvc, selector_,
                          base::BindOnce(&UseCreditCardAction::ExecuteFallback,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void UseCreditCardAction::ExecuteFallback(const ClientStatus& status) {
  DCHECK(fallback_handler_ != nullptr);
  action_stopwatch_.TransferToWaitTime(fallback_handler_->TotalWaitTime());
  fallback_handler_->CheckAndFallbackRequiredFields(
      status, base::BindOnce(&UseCreditCardAction::EndAction,
                             weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace autofill_assistant
