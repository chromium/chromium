// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_MOCK_AUTOFILL_AI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_MOCK_AUTOFILL_AI_DELEGATE_H_

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
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
              OnFormSubmitted,
              (const FormStructure&, ukm::SourceId),
              (override));
  MOCK_METHOD(bool,
              ShouldDisplayIph,
              (autofill::FormGlobalId, autofill::FieldGlobalId),
              (const override));
  MOCK_METHOD(void,
              OnSuggestionsShown,
              (const FormStructure&, const AutofillField&, ukm::SourceId),
              (override));
  MOCK_METHOD(void, OnFormSeen, (const FormStructure&), (override));
  MOCK_METHOD(void,
              OnDidFillSuggestion,
              (const base::Uuid& guid,
               const FormStructure&,
               const AutofillField&,
               base::span<const autofill::AutofillField* const>,
               ukm::SourceId),
              (override));
  MOCK_METHOD(void,
              OnEditedAutofilledField,
              (const FormStructure&, const AutofillField&, ukm::SourceId),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_MOCK_AUTOFILL_AI_DELEGATE_H_
