// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/required_fields_fallback_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

RequiredFieldsFallbackHandler::RequiredFieldsFallbackHandler(
    const std::vector<RequiredField>& required_fields,
    ActionDelegate* action_delegate) {
  required_fields_.assign(required_fields.begin(), required_fields.end());
  action_delegate_ = action_delegate;
}

RequiredFieldsFallbackHandler::~RequiredFieldsFallbackHandler() {}

RequiredFieldsFallbackHandler::FallbackData::FallbackData() {}

RequiredFieldsFallbackHandler::FallbackData::~FallbackData() {}

base::Optional<std::string>
RequiredFieldsFallbackHandler::FallbackData::GetValue(int key) {
  auto it = field_values.find(key);
  if (it != field_values.end()) {
    return it->second;
  }
  return base::nullopt;
}

void RequiredFieldsFallbackHandler::CheckAndFallbackRequiredFields(
    const ClientStatus& initial_autofill_status,
    std::unique_ptr<FallbackData> fallback_data,
    base::OnceCallback<void(const ClientStatus&,
                            const base::Optional<ClientStatus>&)>
        status_update_callback) {
  initial_autofill_status_ = initial_autofill_status;
  status_update_callback_ = std::move(status_update_callback);

  if (required_fields_.empty()) {
    if (!initial_autofill_status.ok()) {
      DVLOG(1) << __func__ << " Autofill failed and no fallback provided "
               << initial_autofill_status.proto_status();
    }

    std::move(status_update_callback_)
        .Run(initial_autofill_status, base::nullopt);
    return;
  }

  CheckAllRequiredFields(std::move(fallback_data));
}

void RequiredFieldsFallbackHandler::CheckAllRequiredFields(
    std::unique_ptr<FallbackData> fallback_data) {
  DCHECK(!batch_element_checker_);
  batch_element_checker_ = std::make_unique<BatchElementChecker>();
  for (size_t i = 0; i < required_fields_.size(); i++) {
    if (required_fields_[i].forced) {
      continue;
    }

    batch_element_checker_->AddFieldValueCheck(
        required_fields_[i].selector,
        base::BindOnce(&RequiredFieldsFallbackHandler::OnGetRequiredFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }

  batch_element_checker_->AddAllDoneCallback(
      base::BindOnce(&RequiredFieldsFallbackHandler::OnCheckRequiredFieldsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(fallback_data)));
  action_delegate_->RunElementChecks(batch_element_checker_.get());
}

void RequiredFieldsFallbackHandler::OnGetRequiredFieldValue(
    size_t required_fields_index,
    const ClientStatus& element_status,
    const std::string& value) {
  required_fields_[required_fields_index].status =
      value.empty() ? EMPTY : NOT_EMPTY;
}

void RequiredFieldsFallbackHandler::OnCheckRequiredFieldsDone(
    std::unique_ptr<FallbackData> fallback_data) {
  batch_element_checker_.reset();

  // We process all fields with an empty value in order to perform the fallback
  // on all those fields, if any.
  bool should_fallback = false;
  for (const RequiredField& required_field : required_fields_) {
    if (required_field.ShouldFallback(fallback_data != nullptr)) {
      should_fallback = true;
      break;
    }
  }

  if (!should_fallback) {
    std::move(status_update_callback_)
        .Run(ClientStatus(ACTION_APPLIED), initial_autofill_status_);
    return;
  }

  if (!fallback_data) {
    // Validation failed and we don't want to try the fallback.
    std::move(status_update_callback_)
        .Run(ClientStatus(MANUAL_FALLBACK), initial_autofill_status_);
    return;
  }

  // If there are any fallbacks for the empty fields, set them, otherwise fail
  // immediately.
  bool has_fallbacks = false;
  for (const RequiredField& required_field : required_fields_) {
    if (!required_field.ShouldFallback(/* has_fallback_data= */ true)) {
      continue;
    }

    if (fallback_data->GetValue(required_field.fallback_key).has_value()) {
      has_fallbacks = true;
    }
  }
  if (!has_fallbacks) {
    std::move(status_update_callback_)
        .Run(ClientStatus(MANUAL_FALLBACK), initial_autofill_status_);
    return;
  }

  // Set the fallback values and check again.
  SetFallbackFieldValuesSequentially(0, std::move(fallback_data));
}

void RequiredFieldsFallbackHandler::SetFallbackFieldValuesSequentially(
    size_t required_fields_index,
    std::unique_ptr<FallbackData> fallback_data) {
  // Skip non-empty fields.
  while (required_fields_index < required_fields_.size() &&
         !required_fields_[required_fields_index].ShouldFallback(
             fallback_data != nullptr)) {
    required_fields_index++;
  }

  // If there are no more fields to set, check the required fields again,
  // but this time we don't want to try the fallback in case of failure.
  if (required_fields_index >= required_fields_.size()) {
    DCHECK_EQ(required_fields_index, required_fields_.size());

    return CheckAllRequiredFields(/* fallback_data= */ nullptr);
  }

  // Set the next field to its fallback value.
  const RequiredField& required_field = required_fields_[required_fields_index];
  auto fallback_value = fallback_data->GetValue(required_field.fallback_key);
  if (!fallback_value.has_value()) {
    DVLOG(3) << "No fallback for " << required_field.selector;
    // If there is no fallback value, we skip this failed field.
    return SetFallbackFieldValuesSequentially(++required_fields_index,
                                              std::move(fallback_data));
  }

  DVLOG(3) << "Setting fallback value for " << required_field.selector;
  action_delegate_->SetFieldValue(
      required_field.selector, fallback_value.value(),
      required_field.simulate_key_presses, required_field.delay_in_millisecond,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), required_fields_index,
                     std::move(fallback_data)));
}

void RequiredFieldsFallbackHandler::OnSetFallbackFieldValue(
    size_t required_fields_index,
    std::unique_ptr<FallbackData> fallback_data,
    const ClientStatus& set_field_status) {
  if (!set_field_status.ok()) {
    // Fallback failed: we stop the script without checking the fields.
    std::move(status_update_callback_)
        .Run(ClientStatus(MANUAL_FALLBACK), initial_autofill_status_);
    return;
  }

  SetFallbackFieldValuesSequentially(++required_fields_index,
                                     std::move(fallback_data));
}
}  // namespace autofill_assistant
