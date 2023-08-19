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
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::AutofillType;
using autofill::CalculateFieldSignatureForField;
using autofill::CalculateFormSignature;
using autofill::FieldGlobalId;
using autofill::FieldSignature;
using autofill::FormData;
using autofill::ServerFieldType;
using autofill::ToSafeServerFieldType;

namespace password_manager {

namespace {

ServerFieldType GetServerType(
    const AutofillType::ServerPrediction& prediction) {
  // The main server predictions is in `field.server_type()` but the server can
  // send additional predictions in `field.server_predictions()`. This function
  // chooses the relevant one for Password Manager predictions.

  // 1. If there is cvc prediction returns it.
  for (const auto& server_predictions : prediction.server_predictions) {
    if (server_predictions.type() == autofill::CREDIT_CARD_VERIFICATION_CODE) {
      return autofill::CREDIT_CARD_VERIFICATION_CODE;
    }
  }

  // 2. If there is password related prediction returns it.
  for (const auto& server_predictions : prediction.server_predictions) {
    ServerFieldType type = ToSafeServerFieldType(
        server_predictions.type(), ServerFieldType::NO_SERVER_DATA);
    if (DeriveFromServerFieldType(type) != CredentialFieldType::kNone) {
      return type;
    }
  }

  // 3. Returns the main prediction.
  return prediction.server_type();
}
}  // namespace

CredentialFieldType DeriveFromServerFieldType(ServerFieldType type) {
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
    default:
      return CredentialFieldType::kNone;
  }
}

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
  for (const auto& field : form.fields) {
    if (auto it = predictions.find(field.global_id());
        it != predictions.end() &&
        it->second.server_type() == autofill::CONFIRMATION_PASSWORD) {
      explicit_confirmation_hint_present = true;
      break;
    }
  }

  std::vector<PasswordFieldPrediction> field_predictions;
  for (const auto& field : form.fields) {
    auto it = predictions.find(field.global_id());
    CHECK(it != predictions.end());
    const AutofillType::ServerPrediction& autofill_prediction = it->second;
    ServerFieldType server_type = GetServerType(autofill_prediction);

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

    field_predictions.emplace_back();
    field_predictions.back().renderer_id = field.unique_renderer_id;
    field_predictions.back().signature = current_signature;
    field_predictions.back().type = server_type;
    field_predictions.back().may_use_prefilled_placeholder =
        autofill_prediction.may_use_prefilled_placeholder;
  }

  FormPredictions result;
  result.driver_id = driver_id;
  result.form_signature = CalculateFormSignature(form);
  result.fields = std::move(field_predictions);
  return result;
}

}  // namespace password_manager
