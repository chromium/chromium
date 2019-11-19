// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_address_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/required_fields_fallback_handler.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {
using RequiredField = RequiredFieldsFallbackHandler::RequiredField;
using FallbackData = RequiredFieldsFallbackHandler::FallbackData;

UseAddressAction::UseAddressAction(ActionDelegate* delegate,
                                   const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_use_address());
  prompt_ = proto.use_address().prompt();
  name_ = proto.use_address().name();
  std::vector<RequiredField> required_fields;
  for (const auto& required_field_proto :
       proto_.use_address().required_fields()) {
    if (required_field_proto.address_field() ==
        UseAddressProto::RequiredField::UNDEFINED) {
      DVLOG(1) << "address_field enum not set, skipping required field";
      continue;
    }

    required_fields.emplace_back();
    RequiredField& required_field = required_fields.back();
    required_field.fallback_key = (int)required_field_proto.address_field();
    required_field.selector = Selector(required_field_proto.element());
    required_field.simulate_key_presses =
        required_field_proto.simulate_key_presses();
    required_field.delay_in_millisecond =
        required_field_proto.delay_in_millisecond();
    required_field.forced = required_field_proto.forced();
  }

  required_fields_fallback_handler_ =
      std::make_unique<RequiredFieldsFallbackHandler>(required_fields,
                                                      delegate);
  selector_ = Selector(proto.use_address().form_field_element());
  selector_.MustBeVisible();
  DCHECK(!selector_.empty());
}

UseAddressAction::~UseAddressAction() = default;

void UseAddressAction::InternalProcessAction(
    ProcessActionCallback action_callback) {
  process_action_callback_ = std::move(action_callback);

  // Ensure data already selected in a previous action.
  auto* client_memory = delegate_->GetClientMemory();
  if (!client_memory->has_selected_address(name_)) {
    auto* error_info = processed_action_proto_->mutable_status_details()
                           ->mutable_autofill_error_info();
    error_info->set_address_key_requested(name_);
    error_info->set_client_memory_address_key_names(
        client_memory->GetAllAddressKeyNames());
    error_info->set_address_pointee_was_null(
        !client_memory->has_selected_address(name_) ||
        !client_memory->selected_address(name_));
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  FillFormWithData();
}

void UseAddressAction::EndAction(
    const ClientStatus& final_status,
    const base::Optional<ClientStatus>& optional_details_status) {
  UpdateProcessedAction(final_status);
  if (optional_details_status.has_value() && !optional_details_status->ok()) {
    processed_action_proto_->mutable_status_details()->MergeFrom(
        optional_details_status->details());
  }
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

void UseAddressAction::FillFormWithData() {
  delegate_->ShortWaitForElement(
      selector_, base::BindOnce(&UseAddressAction::OnWaitForElement,
                                weak_ptr_factory_.GetWeakPtr()));
}

void UseAddressAction::OnWaitForElement(const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(ClientStatus(element_status.proto_status()));
    return;
  }

  DCHECK(!selector_.empty());
  DVLOG(3) << "Retrieving address from client memory under '" << name_ << "'.";
  const autofill::AutofillProfile* profile =
      delegate_->GetClientMemory()->selected_address(name_);
  DCHECK(profile);
  auto fallback_data = CreateFallbackData(*profile);
  delegate_->FillAddressForm(
      profile, selector_,
      base::BindOnce(&UseAddressAction::OnFormFilled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fallback_data)));
}

void UseAddressAction::OnFormFilled(std::unique_ptr<FallbackData> fallback_data,
                                    const ClientStatus& status) {
  required_fields_fallback_handler_->CheckAndFallbackRequiredFields(
      status, std::move(fallback_data),
      base::BindOnce(&UseAddressAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<FallbackData> UseAddressAction::CreateFallbackData(
    const autofill::AutofillProfile& profile) {
  // TODO(crbug.com/806868): Get the locale from the backend.
  std::string app_locale = "en-US";

  auto fallback_data = std::make_unique<FallbackData>();
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::FIRST_NAME,
      base::UTF16ToUTF8(profile.GetInfo(autofill::NAME_FIRST, app_locale)));
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::LAST_NAME,
      base::UTF16ToUTF8(profile.GetInfo(autofill::NAME_LAST, app_locale)));
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::FULL_NAME,
      base::UTF16ToUTF8(profile.GetInfo(autofill::NAME_FIRST, app_locale)));
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::PHONE_NUMBER,
      base::UTF16ToUTF8(
          profile.GetInfo(autofill::PHONE_HOME_WHOLE_NUMBER, app_locale)));
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::EMAIL,
      base::UTF16ToUTF8(profile.GetInfo(autofill::EMAIL_ADDRESS, app_locale)));
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::ORGANIZATION,
      base::UTF16ToUTF8(profile.GetInfo(autofill::COMPANY_NAME, app_locale)));
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::COUNTRY_CODE,
      base::UTF16ToUTF8(
          profile.GetInfo(autofill::ADDRESS_HOME_COUNTRY, app_locale)));
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::REGION,
      base::UTF16ToUTF8(
          profile.GetInfo(autofill::ADDRESS_HOME_STATE, app_locale)));
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::STREET_ADDRESS,
      base::UTF16ToUTF8(
          profile.GetInfo(autofill::ADDRESS_HOME_STREET_ADDRESS, app_locale)));
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::LOCALITY,
      base::UTF16ToUTF8(
          profile.GetInfo(autofill::ADDRESS_HOME_CITY, app_locale)));
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::DEPENDANT_LOCALITY,
      base::UTF16ToUTF8(profile.GetInfo(
          autofill::ADDRESS_HOME_DEPENDENT_LOCALITY, app_locale)));
  fallback_data->field_values.emplace(
      (int)UseAddressProto::RequiredField::POSTAL_CODE,
      base::UTF16ToUTF8(
          profile.GetInfo(autofill::ADDRESS_HOME_ZIP, app_locale)));

  return fallback_data;
}
}  // namespace autofill_assistant
