// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_SUGGESTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_SUGGESTION_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

// Mock version of AutofillSuggestionDelegate.
class MockAutofillSuggestionDelegate : public AutofillSuggestionDelegate {
 public:
  MockAutofillSuggestionDelegate();
  ~MockAutofillSuggestionDelegate() override;

  MOCK_METHOD((absl::variant<AutofillDriver*,
                             password_manager::PasswordManagerDriver*>),
              GetDriver,
              (),
              (override));
  MOCK_METHOD(void,
              OnSuggestionsShown,
              (base::span<const Suggestion>),
              (override));
  MOCK_METHOD(void, OnSuggestionsHidden, (), (override));
  MOCK_METHOD(void,
              DidSelectSuggestion,
              (const Suggestion& suggestion),
              (override));
  MOCK_METHOD(void,
              DidAcceptSuggestion,
              (const Suggestion& suggestion,
               const AutofillSuggestionDelegate::SuggestionMetadata& metadata),
              (override));
  MOCK_METHOD(void,
              DidPerformButtonActionForSuggestion,
              (const Suggestion&, const SuggestionButtonAction&),
              (override));
  MOCK_METHOD(bool, RemoveSuggestion, (const Suggestion&), (override));
  MOCK_METHOD(void, ClearPreviewedForm, (), (override));
  MOCK_METHOD(FillingProduct, GetMainFillingProduct, (), (const, override));

  base::WeakPtr<MockAutofillSuggestionDelegate> GetWeakPtr();

 private:
  base::WeakPtrFactory<MockAutofillSuggestionDelegate> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_SUGGESTION_DELEGATE_H_
