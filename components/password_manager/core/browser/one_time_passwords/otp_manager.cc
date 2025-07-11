// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/one_time_passwords/otp_manager.h"

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/one_time_passwords/otp_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

namespace {

using autofill::FieldGlobalId;
using autofill::FormGlobalId;

std::vector<autofill::FieldGlobalId> GetFillableOtpFieldIds(
    const autofill::FormData& form,
    const base::flat_map<autofill::FieldGlobalId, autofill::FieldType>&
        field_predictions) {
  std::vector<autofill::FieldGlobalId> fillable_otp_fields;
  for (const auto& prediction : field_predictions) {
    if (prediction.second != autofill::ONE_TIME_CODE) {
      continue;
    }
    const autofill::FormFieldData* field =
        form.FindFieldByGlobalId(prediction.first);
    if (field->IsTextInputElement()) {
      fillable_otp_fields.push_back(prediction.first);
    }
  }
  return fillable_otp_fields;
}

}  // namespace

OtpManager::OtpManager(PasswordManagerClient* client) : client_(client) {
  CHECK(client_);
}

OtpManager::~OtpManager() = default;

void OtpManager::ProcessClassificationModelPredictions(
    const autofill::FormData& form,
    const base::flat_map<autofill::FieldGlobalId, autofill::FieldType>&
        field_predictions) {
  std::vector<autofill::FieldGlobalId> fillable_otp_fields(
      GetFillableOtpFieldIds(form, field_predictions));

  autofill::FormGlobalId form_id(form.global_id());
  if (fillable_otp_fields.empty()) {
    form_managers_.erase(form_id);
    return;
  }

  OtpFormManager* form_manager = GetManagerForForm(form_id);
  if (!form_manager) {
    form_managers_.emplace(form_id, std::make_unique<OtpFormManager>(
                                        form_id, fillable_otp_fields, client_));
    client_->InformPasswordChangeServiceOfOtpPresent();

  } else {
    form_manager->ProcessUpdatedPredictions(fillable_otp_fields);
  }
}

bool OtpManager::IsFieldEligibleForOtpFilling(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id) const {
  OtpFormManager* form_manager = GetManagerForForm(form_id);
  return form_manager ? form_manager->IsFieldEligibleForOtpFilling(field_id)
                      : false;
}

void OtpManager::GetOtpSuggestions(
    const autofill::FormGlobalId& form_id,
    const autofill::FieldGlobalId& field_id,
    base::OnceCallback<void(std::vector<std::string>)> callback) const {
  OtpFormManager* form_manager = GetManagerForForm(form_id);
  CHECK(form_manager);
  form_manager->GetOtpSuggestions(field_id, std::move(callback));
}

OtpFormManager* OtpManager::GetManagerForForm(
    const FormGlobalId& form_id) const {
  if (form_managers_.find(form_id) == form_managers_.end()) {
    return nullptr;
  }
  return form_managers_.at(form_id).get();
}

}  // namespace password_manager
