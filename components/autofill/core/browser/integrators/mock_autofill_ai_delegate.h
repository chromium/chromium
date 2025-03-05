// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_MOCK_AUTOFILL_AI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_MOCK_AUTOFILL_AI_DELEGATE_H_

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/autofill_ai_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillAiDelegate : public AutofillAiDelegate {
 public:
  MockAutofillAiDelegate();
  ~MockAutofillAiDelegate() override;

  MOCK_METHOD(std::vector<autofill::Suggestion>,
              GetSuggestions,
              (autofill::FormGlobalId, autofill::FieldGlobalId),
              (override));
  MOCK_METHOD(bool,
              IsFormAndFieldEligibleForAutofillAi,
              (const FormStructure&, const AutofillField&),
              (const override));
  MOCK_METHOD(bool, IsUserEligible, (), (const override));
  MOCK_METHOD(bool, IsUserEligibleForFillingAndImporting, (), (const override));
  MOCK_METHOD(bool, MaybeImportForm, (const FormStructure&), (override));
  MOCK_METHOD(bool,
              ShouldDisplayIph,
              (const AutofillField& field),
              (const override));
  MOCK_METHOD(void,
              OnSuggestionsShown,
              (const DenseSet<SuggestionType>&, const FormGlobalId&),
              (override));
  MOCK_METHOD(void, OnFormSeen, (const FormStructure&), (override));
  MOCK_METHOD(void, OnDidFillSuggestion, (FormGlobalId), (override));
  MOCK_METHOD(void, OnEditedAutofilledField, (FormGlobalId), (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_MOCK_AUTOFILL_AI_DELEGATE_H_
