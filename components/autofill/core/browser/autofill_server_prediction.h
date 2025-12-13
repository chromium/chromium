// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SERVER_PREDICTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SERVER_PREDICTION_H_

#include <optional>
#include <string>
#include <vector>

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"

namespace autofill {

class AutofillField;

// A collection of server prediction metadata related to a form field.
// Its current intended use is solely for consumers outside of
// components/autofill.
//
// Avoid including this file -- try to forward declare instead.
//
// It is in a separate header because of its compile size cost due to the
// included protobuf headers.
struct AutofillServerPrediction {
  AutofillServerPrediction();
  explicit AutofillServerPrediction(const AutofillField& field);

  AutofillServerPrediction(const AutofillServerPrediction&);
  AutofillServerPrediction& operator=(const AutofillServerPrediction&);
  AutofillServerPrediction(AutofillServerPrediction&&);
  AutofillServerPrediction& operator=(AutofillServerPrediction&&);

  ~AutofillServerPrediction();

  // The most likely server-side prediction for the field's type.
  FieldType server_type() const;

  // Checks whether server-side prediction for the field's type is an
  // override.
  bool is_override() const;

  // Requirements the site imposes on passwords (for password generation)
  // obtained from the Autofill server.
  std::optional<PasswordRequirementsSpec> password_requirements;

  // The server-side predictions for the field's type.
  std::vector<
      AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction>
      server_predictions;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SERVER_PREDICTION_H_
