// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_credit_card_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
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
#include "components/autofill_assistant/browser/web/element_action_util.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "components/autofill_assistant/core/public/autofill_assistant_intent.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {
namespace {

bool SkipAutofill(const UseCreditCardProto& proto) {
  return proto.skip_autofill() || proto.skip_resolve();
}

}  // namespace

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

  if (selector_.empty() && !SkipAutofill(proto_.use_card())) {
    VLOG(1) << "UseCreditCard failed: |selector| empty";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }
  if (SkipAutofill(proto_.use_card()) &&
      proto_.use_card().required_fields().empty()) {
    VLOG(1)
        << "UseCreditCard failed: Skipping Autofill without required fields";
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
        credit_card_value->credit_cards().values(0));
    if (credit_card == nullptr) {
      VLOG(1) << "UseCreditCard failed: card not found for: "
              << *credit_card_value;
      EndAction(ClientStatus(PRECONDITION_FAILED));
      return;
    }
    credit_card_ = std::make_unique<autofill::CreditCard>(*credit_card);
  } else {
    const auto* credit_card = delegate_->GetUserData()->selected_card();
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
    DCHECK(SkipAutofill(proto_.use_card()));
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

  if (proto_.use_card().skip_resolve()) {
    DCHECK(credit_card_);
    InitFallbackHandler(*credit_card_, std::u16string(),
                        /* is_resolved= */ false);
    ExecuteFallback(OkClientStatus());
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

  InitFallbackHandler(*card, cvc, /* is_resolved= */ true);

  if (proto_.use_card().skip_autofill()) {
    ExecuteFallback(OkClientStatus());
    return;
  }

  DCHECK(!selector_.empty());
  delegate_->FindElement(
      selector_,
      base::BindOnce(
          &element_action_util::TakeElementAndPerform,
          base::BindOnce(&WebController::FillCardForm,
                         delegate_->GetWebController()->GetWeakPtr(),
                         std::move(card),
                         ExtractIntentFromString(delegate_->GetIntent()), cvc),
          base::BindOnce(&UseCreditCardAction::ExecuteFallback,
                         weak_ptr_factory_.GetWeakPtr())));
}

void UseCreditCardAction::InitFallbackHandler(const autofill::CreditCard& card,
                                              const std::u16string& cvc,
                                              bool is_resolved) {
  std::vector<RequiredField> required_fields;
  for (const auto& required_field_proto : proto_.use_card().required_fields()) {
    if (!required_field_proto.has_value_expression()) {
      continue;
    }

    RequiredField required_field;
    required_field.FromProto(required_field_proto);
    required_fields.emplace_back(required_field);
  }

  auto fallback_values =
      field_formatter::CreateAutofillMappings(card,
                                              /* locale= */ "en-US");

  if (is_resolved) {
    fallback_values.emplace(
        field_formatter::Key(
            AutofillFormatProto::CREDIT_CARD_VERIFICATION_CODE),
        base::UTF16ToUTF8(cvc));
    fallback_values.emplace(
        field_formatter::Key(AutofillFormatProto::CREDIT_CARD_RAW_NUMBER),
        base::UTF16ToUTF8(card.GetRawInfo(autofill::CREDIT_CARD_NUMBER)));
  } else {
    fallback_values.erase(field_formatter::Key(autofill::CREDIT_CARD_NUMBER));
  }

  DCHECK(fallback_handler_ == nullptr);
  fallback_handler_ = std::make_unique<RequiredFieldsFallbackHandler>(
      required_fields, fallback_values, delegate_);
}

void UseCreditCardAction::ExecuteFallback(const ClientStatus& status) {
  DCHECK(fallback_handler_ != nullptr);
  action_stopwatch_.TransferToWaitTime(fallback_handler_->TotalWaitTime());
  fallback_handler_->CheckAndFallbackRequiredFields(
      status, base::BindOnce(&UseCreditCardAction::EndAction,
                             weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace autofill_assistant
