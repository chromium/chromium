// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_

#include "components/autofill/core/browser/autofill_prediction_improvements_delegate.h"
#include "components/autofill/core/browser/form_structure.h"
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

  MOCK_METHOD(bool,
              MaybeUpdateSuggestions,
              (std::vector<Suggestion> & address_suggestions,
               const FormFieldData& field,
               bool should_add_trigger_suggestion),
              (override));
  MOCK_METHOD(bool,
              ShouldProvidePredictionImprovements,
              (const GURL& url),
              (override));
  MOCK_METHOD(void,
              UserFeedbackReceived,
              (AutofillPredictionImprovementsDelegate::UserFeedback feedback),
              (override));
  MOCK_METHOD(bool,
              IsFormEligible,
              (const autofill::FormStructure& form),
              (override));
  MOCK_METHOD(void, UserClickedLearnMore, (), (override));
  MOCK_METHOD(void,
              OnClickedTriggerSuggestion,
              (const autofill::FormData& form,
               const autofill::FormFieldData& trigger_field,
               UpdateSuggestionsCallback update_suggestions_callback),
              (override));
  MOCK_METHOD(void,
              MaybeImportForm,
              (const autofill::FormData& form,
               const autofill::FormStructure& form_structure,
               ImportFormCallback callback),
              (override));
  MOCK_METHOD(void, HasDataStored, (HasDataCallback callback), (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_
