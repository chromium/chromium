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

std::vector<FieldGlobalId> GetFillableOtpFieldIds(
    const autofill::FormData& form,
    const base::flat_map<FieldGlobalId, autofill::FieldType>&
        field_predictions) {
  std::vector<FieldGlobalId> fillable_otp_fields;
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

std::pair<std::vector<FieldGlobalId>, std::vector<FieldGlobalId>>
GetOverridePredictions(
    const base::flat_map<FieldGlobalId,
                         autofill::AutofillType::ServerPrediction>&
        field_predictions) {
  std::vector<FieldGlobalId> otp_overrides;
  std::vector<FieldGlobalId> other_overrides;

  for (const auto& [field_id, prediction] : field_predictions) {
    if (prediction.is_override()) {
      if (prediction.server_type() == autofill::ONE_TIME_CODE) {
        otp_overrides.push_back(field_id);
      } else {
        other_overrides.push_back(field_id);
      }
    }
  }
  return {otp_overrides, other_overrides};
}

}  // namespace

OtpManager::OtpManager(PasswordManagerClient* client) : client_(client) {
  CHECK(client_);
}

OtpManager::~OtpManager() = default;

void OtpManager::ProcessClassificationModelPredictions(
    const autofill::FormData& form,
    const base::flat_map<FieldGlobalId, autofill::FieldType>&
        field_predictions) {
  std::vector<FieldGlobalId> fillable_otp_fields(
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
    for (Observer& observer : observers_) {
      observer.OnOtpFieldDetected(GetManagerForForm(form_id));
    }
    client_->InformPasswordChangeServiceOfOtpPresent();

  } else {
    form_manager->ProcessUpdatedPredictions(fillable_otp_fields);
  }
}

void OtpManager::ProcessServerPredictions(
    const autofill::FormData& form,
    const base::flat_map<FieldGlobalId,
                         autofill::AutofillType::ServerPrediction>&
        field_predictions) {
  // The server does not classify OTP fields, but it can provide manual
  // overrides.
  auto [otp_overrides, other_overrides] =
      GetOverridePredictions(field_predictions);

  OtpFormManager* form_manager = GetManagerForForm(form.global_id());
  if (!form_manager) {
    if (otp_overrides.empty()) {
      // Return early if the form was not predicted to be an OTP form
      // neither by the classification model, nor by the server.
      return;
    }
    // Create a new form manager if the form was predicted to be an OTP form by
    // the server.
    form_managers_.emplace(form.global_id(),
                           std::make_unique<OtpFormManager>(
                               form.global_id(), otp_overrides, client_));
    for (Observer& observer : observers_) {
      observer.OnOtpFieldDetected(GetManagerForForm(form.global_id()));
    }
    client_->InformPasswordChangeServiceOfOtpPresent();
    return;
  }

  form_manager->ProcessServerOverrides(otp_overrides, other_overrides);
  if (form_manager->otp_field_ids().empty()) {
    // Destroy the manager if no OTP fields are left.
    form_managers_.erase(form.global_id());
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

void OtpManager::OnRenderFrameDeleted(
    const autofill::LocalFrameToken& frame_token) {
  CleanFormManagersForTheFrame(frame_token);
}

void OtpManager::OnDidFinishNavigationInMainFrame() {
  // If navigation happens in the main frame, all child frames also become
  // inaccessible, but they are not guaranteed to be deleted timely, therefore
  // it's better to clean all form managers cache now.
  form_managers_.clear();
}

void OtpManager::OnDidFinishNavigationInIframe(
    const autofill::LocalFrameToken& frame_token) {
  CleanFormManagersForTheFrame(frame_token);
}

OtpFormManager* OtpManager::GetManagerForForm(
    const FormGlobalId& form_id) const {
  if (form_managers_.find(form_id) == form_managers_.end()) {
    return nullptr;
  }
  return form_managers_.at(form_id).get();
}

void OtpManager::CleanFormManagersForTheFrame(
    const autofill::LocalFrameToken& frame_token) {
  base::EraseIf(form_managers_, ([&](const auto& manager) {
                  return manager.first.frame_token == frame_token;
                }));
}

}  // namespace password_manager
