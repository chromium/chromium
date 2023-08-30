// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/browser_autofill_manager_test_delegate.h"

namespace autofill {

BrowserAutofillManagerTestDelegate::BrowserAutofillManagerTestDelegate() =
    default;
BrowserAutofillManagerTestDelegate::~BrowserAutofillManagerTestDelegate() =
    default;

void BrowserAutofillManagerTestDelegate::Observe(AutofillManager& manager) {
  if (!observations_.IsObservingSource(&manager)) {
    observations_.AddObservation(&manager);
  }
}

void BrowserAutofillManagerTestDelegate::OnAutofillManagerDestroyed(
    AutofillManager& manager) {
  observations_.RemoveObservation(&manager);
}

void BrowserAutofillManagerTestDelegate::OnFillOrPreviewDataModelForm(
    AutofillManager& manager,
    FormGlobalId form,
    mojom::AutofillActionPersistence action_persistence,
    base::span<const std::pair<const FormFieldData*, const AutofillField*>>
        filled_fields,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card) {
  switch (action_persistence) {
    case mojom::AutofillActionPersistence::kFill:
      DidFillFormData();
      break;
    case mojom::AutofillActionPersistence::kPreview:
      DidPreviewFormData();
      break;
  }
}

void BrowserAutofillManagerTestDelegate::OnSuggestionsShown(
    AutofillManager& manager) {
  DidShowSuggestions();
}

void BrowserAutofillManagerTestDelegate::OnSuggestionsHidden(
    AutofillManager& manager) {
  DidHideSuggestions();
}

}  // namespace autofill
