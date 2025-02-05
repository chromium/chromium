// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ranges>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"

namespace autofill_ai {

using autofill::AutofillField;
using autofill::FieldTypeGroup;

bool IsFormEligibleForFilling(const autofill::FormStructure& form) {
  return std::ranges::any_of(
      form.fields(), [](const std::unique_ptr<AutofillField>& field) {
        return field->GetAutofillAiServerTypePredictions().has_value();
      });
}

}  // namespace autofill_ai
