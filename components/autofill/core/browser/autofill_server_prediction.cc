// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_server_prediction.h"

#include <optional>
#include <vector>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

AutofillServerPrediction::AutofillServerPrediction() = default;

AutofillServerPrediction::AutofillServerPrediction(const AutofillField& field)
    : password_requirements(field.password_requirements()),
      server_predictions(field.server_predictions()) {}

AutofillServerPrediction::AutofillServerPrediction(
    const AutofillServerPrediction&) = default;

AutofillServerPrediction& AutofillServerPrediction::operator=(
    const AutofillServerPrediction&) = default;

AutofillServerPrediction::AutofillServerPrediction(AutofillServerPrediction&&) =
    default;

AutofillServerPrediction& AutofillServerPrediction::operator=(
    AutofillServerPrediction&&) = default;

AutofillServerPrediction::~AutofillServerPrediction() = default;

FieldType AutofillServerPrediction::server_type() const {
  return server_predictions.empty()
             ? NO_SERVER_DATA
             : ToSafeFieldType(server_predictions[0].type())
                   .value_or(NO_SERVER_DATA);
}

bool AutofillServerPrediction::is_override() const {
  return !server_predictions.empty() && server_predictions[0].override();
}

}  // namespace autofill
