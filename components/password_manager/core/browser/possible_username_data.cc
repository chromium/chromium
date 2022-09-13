// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/possible_username_data.h"

#include <string>

#include "base/strings/string_piece.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"


namespace password_manager {

namespace {

// Find a field in |predictions| with given renderer id.
const PasswordFieldPrediction* FindFieldPrediction(
    const FormPredictions& predictions,
    autofill::FieldRendererId field_renderer_id) {
  for (const auto& field : predictions.fields) {
    if (field.renderer_id == field_renderer_id)
      return &field;
  }
  return nullptr;
}

}  // namespace

PossibleUsernameData::PossibleUsernameData(
    std::string signon_realm,
    autofill::FieldRendererId renderer_id,
    const std::u16string& field_name,
    const std::u16string& value,
    base::Time last_change,
    int driver_id)
    : signon_realm(std::move(signon_realm)),
      renderer_id(renderer_id),
      field_name(field_name),
      value(value),
      last_change(last_change),
      driver_id(driver_id) {}
PossibleUsernameData::PossibleUsernameData(const PossibleUsernameData&) =
    default;
PossibleUsernameData::~PossibleUsernameData() = default;

bool PossibleUsernameData::IsStale() const {
  return base::Time::Now() - last_change > kPossibleUsernameExpirationTimeout;
}

bool PossibleUsernameData::HasSingleUsernameServerPrediction() const {
  // Check if there is a server prediction.
  if (!form_predictions)
    return false;
  const PasswordFieldPrediction* field_prediction =
      FindFieldPrediction(*form_predictions, renderer_id);
  return field_prediction &&
         field_prediction->type == autofill::SINGLE_USERNAME;
}

}  // namespace password_manager
