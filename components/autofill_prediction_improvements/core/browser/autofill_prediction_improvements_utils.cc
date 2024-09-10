// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill_prediction_improvements {

bool IsFormEligibleByFieldCriteria(const autofill::FormStructure& form) {
  // A counter for the number of fields that have been identified as eligible.
  int prediction_improvement_eligable_fields = 0;
  // Address fields that are not also eligible for prediction improvements.
  int additional_address_fields = 0;

  for (const auto& field : form) {
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    if (field->heuristic_type(
            autofill::HeuristicSource::kPredictionImprovementRegexes) ==
        autofill::IMPROVED_PREDICTION) {
      ++prediction_improvement_eligable_fields;
    }
#else
    if (field->Type().GetStorableType() == autofill::IMPROVED_PREDICTION) {
      ++prediction_improvement_eligable_fields;
    }
#endif
    else if (autofill::IsAddressType(field->Type().GetStorableType())) {
      ++additional_address_fields;
    }
  }

  const int total_number_of_fillable_fields =
      prediction_improvement_eligable_fields + additional_address_fields;

  // TODO(crbug.com/365517792): Make this controllable via finch.
  return prediction_improvement_eligable_fields > 0 &&
         total_number_of_fillable_fields > 0;
}

}  // namespace autofill_prediction_improvements
