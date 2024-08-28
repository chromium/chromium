// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::AutofillType;
using autofill::CalculateFieldSignatureForField;
using autofill::CalculateFormSignature;
using autofill::FieldGlobalId;
using autofill::FieldSignature;
using autofill::FieldType;
using autofill::FormData;
using autofill::ToSafeFieldType;

namespace password_manager {

namespace {

FieldType GetServerType(const AutofillType::ServerPrediction& prediction) {
  // The main server predictions is in `field.server_type()` but the server can
  // send additional predictions in `field.server_predictions()`. This function
  // chooses the relevant one for Password Manager predictions.

  // 1. If there is credit card related prediction, return the prediction.
  for (const auto& server_predictions : prediction.server_predictions) {
    FieldType type = static_cast<FieldType>(server_predictions.type());
    if (GroupTypeOfFieldType(type) == autofill::FieldTypeGroup::kCreditCard) {
      return type;
    }
  }

  // 2. If there is password related prediction returns it.
  for (const auto& server_predictions : prediction.server_predictions) {
    FieldType type = static_cast<FieldType>(server_predictions.type());
    if (DeriveFromFieldType(type) != CredentialFieldType::kNone) {
      return type;
    }
  }

  // 3. Returns the main prediction.
  return prediction.server_type();
}
}  // namespace

CredentialFieldType DeriveFromFieldType(FieldType type) {
  if (GroupTypeOfFieldType(type) == autofill::FieldTypeGroup::kCreditCard) {
    return CredentialFieldType::kNonCredential;
  }
  // TODO: crbug/40925827 - Move if statement under switch case after the
  // feature is launched.
  if (type == autofill::SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES &&
      base::FeatureList::IsEnabled(
          features::kUsernameFirstFlowWithIntermediateValuesPredictions)) {
    return CredentialFieldType::kSingleUsername;
  }
  switch (type) {
    case autofill::USERNAME:
    case autofill::USERNAME_AND_EMAIL_ADDRESS:
      return CredentialFieldType::kUsername;
    case autofill::SINGLE_USERNAME:
    case autofill::SINGLE_USERNAME_FORGOT_PASSWORD:
      return CredentialFieldType::kSingleUsername;
    case autofill::PASSWORD:
      return CredentialFieldType::kCurrentPassword;
    case autofill::ACCOUNT_CREATION_PASSWORD:
    case autofill::NEW_PASSWORD:
      return CredentialFieldType::kNewPassword;
    case autofill::CONFIRMATION_PASSWORD:
      return CredentialFieldType::kConfirmationPassword;
    case autofill::NOT_PASSWORD:
    case autofill::NOT_USERNAME:
    case autofill::ONE_TIME_CODE:
      return CredentialFieldType::kNonCredential;
    default:
      return CredentialFieldType::kNone;
  }
}

PasswordFieldPrediction::PasswordFieldPrediction(
    autofill::FieldRendererId renderer_id,
    autofill::FieldSignature signature,
    autofill::FieldType type,
    bool may_use_prefilled_placeholder,
    bool is_override)
    : renderer_id(renderer_id),
      signature(signature),
      type(ToSafeFieldType(type, FieldType::NO_SERVER_DATA)),
      may_use_prefilled_placeholder(may_use_prefilled_placeholder),
      is_override(is_override) {}

PasswordFieldPrediction::PasswordFieldPrediction(
    const PasswordFieldPrediction&) = default;
PasswordFieldPrediction& PasswordFieldPrediction::operator=(
    const PasswordFieldPrediction&) = default;
PasswordFieldPrediction::PasswordFieldPrediction(PasswordFieldPrediction&&) =
    default;
PasswordFieldPrediction& PasswordFieldPrediction::operator=(
    PasswordFieldPrediction&&) = default;
PasswordFieldPrediction::~PasswordFieldPrediction() = default;

FormPredictions::FormPredictions() = default;
FormPredictions::FormPredictions(const FormPredictions&) = default;
FormPredictions& FormPredictions::operator=(const FormPredictions&) = default;
FormPredictions::FormPredictions(FormPredictions&&) = default;
FormPredictions& FormPredictions::operator=(FormPredictions&&) = default;
FormPredictions::~FormPredictions() = default;

FormPredictions ConvertToFormPredictions(
    int driver_id,
    const autofill::FormData& form,
    const base::flat_map<FieldGlobalId, AutofillType::ServerPrediction>&
        predictions) {
  // This is a mostly mechanical transformation, except for the following case:
  // If there is no explicit CONFIRMATION_PASSWORD field, and there are two
  // fields with the same signature and one of the "new password" types, then
  // the latter of those two should be marked as CONFIRMATION_PASSWORD. For
  // fields which have the same signature, the server has no means to hint
  // different types, and it is likely that one of them is the confirmation
  // field.

  // Stores the signature of the last field with the server type
  // ACCOUNT_CREATION_PASSWORD or NEW_PASSWORD. Initially,
  // |last_new_password|.is_null() to represents "no
  // field with the 'new password' type as been seen yet".
  FieldSignature last_new_password;

  bool explicit_confirmation_hint_present = false;
  for (const auto& field : form.fields()) {
    if (auto it = predictions.find(field.global_id());
        it != predictions.end() &&
        it->second.server_type() == autofill::CONFIRMATION_PASSWORD) {
      explicit_confirmation_hint_present = true;
      break;
    }
  }

  std::vector<PasswordFieldPrediction> field_predictions;
  for (const auto& field : form.fields()) {
    auto it = predictions.find(field.global_id());
    CHECK(it != predictions.end());
    const AutofillType::ServerPrediction& autofill_prediction = it->second;
    FieldType server_type = GetServerType(autofill_prediction);

    FieldSignature current_signature = CalculateFieldSignatureForField(field);

    if (!explicit_confirmation_hint_present &&
        (server_type == autofill::ACCOUNT_CREATION_PASSWORD ||
         server_type == autofill::NEW_PASSWORD)) {
      if (last_new_password && last_new_password == current_signature) {
        server_type = autofill::CONFIRMATION_PASSWORD;
      } else {
        last_new_password = current_signature;
      }
    }

    field_predictions.emplace_back(
        field.renderer_id(), current_signature, server_type,
        /*may_use_prefilled_placeholder=*/
        autofill_prediction.may_use_prefilled_placeholder.value_or(false),
        /*is_override=*/autofill_prediction.is_override());
  }

  FormPredictions result;
  result.driver_id = driver_id;
  result.form_signature = CalculateFormSignature(form);
  result.fields = std::move(field_predictions);
  return result;
}

}  // namespace password_manager
