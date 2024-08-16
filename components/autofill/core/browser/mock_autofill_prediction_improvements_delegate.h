// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_

#include "components/autofill/core/browser/autofill_prediction_improvements_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillPredictionImprovementsDelegate
    : public AutofillPredictionImprovementsDelegate {
 public:
  MockAutofillPredictionImprovementsDelegate();
  ~MockAutofillPredictionImprovementsDelegate() override;

  MOCK_METHOD(std::vector<Suggestion>,
              GetSuggestions,
              (const FormFieldData& field),
              (override));
  MOCK_METHOD(bool,
              HasImprovedPredictionsForField,
              (const FormFieldData& field),
              (override));
  MOCK_METHOD(bool,
              UsedImprovedPredictionsForField,
              (const FormFieldData& field),
              (override));
  MOCK_METHOD(void,
              ExtractImprovedPredictionsForFormFields,
              (const FormData& form, FillPredictionsCallback fill_callback),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_
