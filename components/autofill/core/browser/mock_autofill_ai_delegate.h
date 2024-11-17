// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_AI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_AI_DELEGATE_H_

#include "components/autofill/core/browser/autofill_ai_delegate.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillAiDelegate : public AutofillAiDelegate {
 public:
  MockAutofillAiDelegate();
  ~MockAutofillAiDelegate() override;

  MOCK_METHOD(std::vector<Suggestion>,
              GetSuggestions,
              (const std::vector<Suggestion>& address_suggestions,
               const FormData& form,
               const FormFieldData& field),
              (override));
  MOCK_METHOD(void, UserFeedbackReceived, (UserFeedback feedback), (override));
  MOCK_METHOD(bool,
              IsEligibleForAutofillAi,
              (const FormStructure& form, const AutofillField& field),
              (const override));
  MOCK_METHOD(bool, IsUserEligible, (), (const override));
  MOCK_METHOD(void, UserClickedLearnMore, (), (override));
  MOCK_METHOD(void,
              OnClickedTriggerSuggestion,
              (const FormData& form,
               const FormFieldData& trigger_field,
               UpdateSuggestionsCallback update_suggestions_callback),
              (override));
  MOCK_METHOD(
      void,
      MaybeImportForm,
      (std::unique_ptr<FormStructure> form,
       base::OnceCallback<void(std::unique_ptr<FormStructure> form,
                               bool autofill_ai_shows_bubble)> callback),
      (override));
  MOCK_METHOD(void, HasDataStored, (HasDataCallback callback), (override));
  MOCK_METHOD(bool,
              ShouldDisplayIph,
              (const FormStructure& form, const AutofillField& field),
              (const override));
  MOCK_METHOD(void, GoToSettings, (), (const override));
  MOCK_METHOD(void,
              OnSuggestionsShown,
              (const DenseSet<SuggestionType>& shown_suggestion_types,
               const FormData& form,
               const FormFieldData& trigger_field,
               UpdateSuggestionsCallback update_suggestions_callback),
              (override));
  MOCK_METHOD(void, OnFormSeen, (const FormStructure& form), (override));
  MOCK_METHOD(void, OnDidFillSuggestion, (FormGlobalId form_id), (override));
  MOCK_METHOD(void,
              OnEditedAutofilledField,
              (FormGlobalId form_id),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_AI_DELEGATE_H_
