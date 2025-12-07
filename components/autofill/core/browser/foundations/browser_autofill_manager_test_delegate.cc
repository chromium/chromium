// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_delegate.h"

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

void BrowserAutofillManagerTestDelegate::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillManager::LifecycleState old_state,
    AutofillManager::LifecycleState new_state) {
  switch (new_state) {
    case AutofillManager::LifecycleState::kInactive:
    case AutofillManager::LifecycleState::kActive:
    case AutofillManager::LifecycleState::kPendingReset:
      break;
    case AutofillManager::LifecycleState::kPendingDeletion:
      observations_.RemoveObservation(&manager);
      break;
  }
}

void BrowserAutofillManagerTestDelegate::OnFillOrPreviewForm(
    AutofillManager& manager,
    FormGlobalId form_id,
    mojom::ActionPersistence action_persistence,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const FillingPayload& filling_payload) {
  switch (action_persistence) {
    case mojom::ActionPersistence::kFill:
      DidFillFormData();
      break;
    case mojom::ActionPersistence::kPreview:
      DidPreviewFormData();
      break;
  }
}

void BrowserAutofillManagerTestDelegate::OnSuggestionsShown(
    AutofillManager& manager,
    base::span<const Suggestion> suggestions) {
  DidShowSuggestions();
}

void BrowserAutofillManagerTestDelegate::OnSuggestionsHidden(
    AutofillManager& manager) {
  DidHideSuggestions();
}

}  // namespace autofill
