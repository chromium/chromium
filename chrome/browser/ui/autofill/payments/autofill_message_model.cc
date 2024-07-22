// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_message_model.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"
#include "components/grit/components_scaled_resources.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"

namespace autofill {

AutofillMessageModel::AutofillMessageModel(
    std::unique_ptr<messages::MessageWrapper> message,
    Type type)
    : message_(std::move(message)), type_(type) {}

AutofillMessageModel::~AutofillMessageModel() = default;

AutofillMessageModel::AutofillMessageModel(AutofillMessageModel&&) = default;

AutofillMessageModel& AutofillMessageModel::operator=(AutofillMessageModel&&) =
    default;

std::unique_ptr<AutofillMessageModel>
AutofillMessageModel::CreateForSaveCardFailure() {
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params =
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
          CreateForSaveCardFailure();

  std::unique_ptr<messages::MessageWrapper> message =
      std::make_unique<messages::MessageWrapper>(
          messages::MessageIdentifier::SAVE_CARD_FAILURE);
  message->SetTitle(ui_params.title_text);
  message->SetDescription(ui_params.description_text);
  message->SetPrimaryButtonText(ui_params.failure_ok_button_text);
  message->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_CC_GENERIC));

  return base::WrapUnique(
      new AutofillMessageModel(std::move(message), Type::kSaveCardFailure));
}

std::unique_ptr<AutofillMessageModel>
AutofillMessageModel::CreateForVirtualCardEnrollFailure(
    std::u16string card_label) {
  SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params =
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams::
          CreateForVirtualCardFailure(card_label);

  std::unique_ptr<messages::MessageWrapper> message =
      std::make_unique<messages::MessageWrapper>(
          messages::MessageIdentifier::VIRTUAL_CARD_ENROLL_FAILURE);
  message->SetTitle(ui_params.title_text);
  message->SetDescription(ui_params.description_text);
  message->SetPrimaryButtonText(ui_params.failure_ok_button_text);
  message->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_CC_GENERIC));

  return base::WrapUnique(new AutofillMessageModel(
      std::move(message), Type::kVirtualCardEnrollFailure));
}

messages::MessageWrapper& AutofillMessageModel::GetMessage(
    base::PassKey<AutofillMessageController> pass_key) {
  return *message_;
}

const AutofillMessageModel::Type& AutofillMessageModel::GetType() const {
  return type_;
}

std::string_view AutofillMessageModel::GetTypeAsString() const {
  return TypeToString(type_);
}

std::string_view AutofillMessageModel::TypeToString(Type message_type) {
  switch (message_type) {
    case Type::kUnspecified:
      return "Unspecified";
    case Type::kSaveCardFailure:
      return "SaveCardFailure";
    case Type::kVirtualCardEnrollFailure:
      return "VirtualCardEnrollFailure";
  }
}

}  // namespace autofill
