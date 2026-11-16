// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_BROWSER_AUTOFILL_MANAGER_TEST_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_BROWSER_AUTOFILL_MANAGER_TEST_DELEGATE_H_

#include "base/scoped_multi_source_observation.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"

namespace autofill {

// Deprecated. Use AutofillManager::Observer instead, especially WaitForEvent()
// instead.
// TODO(crbug.com/40279936): Remove this class.
class BrowserAutofillManagerTestDelegate : public AutofillManager::Observer {
 public:
  BrowserAutofillManagerTestDelegate();
  ~BrowserAutofillManagerTestDelegate() override;

  void Observe(AutofillManager& manager);

  // Called when a form is previewed with Autofill suggestions, i.e., when the
  // previewed values were sent to the renderer.
  virtual void DidPreviewFormData() = 0;

  // Called when a form is filled with Autofill suggestions, i.e., when the
  // filled values were sent to the renderer. This happens before the
  // browser-side post-fill bookkeeping (e.g., cache update, undo history,
  // metrics), which may run in a later task.
  virtual void DidFillFormData() = 0;

  // Called when a popup with Autofill suggestions is shown.
  virtual void DidShowSuggestions() = 0;

  // Called when a popup with Autofill suggestions is hidden.
  virtual void DidHideSuggestions() = 0;

 private:
  // AutofillManager::Observer:
  void OnAutofillManagerStateChanged(
      AutofillManager& manager,
      AutofillManager::LifecycleState old_state,
      AutofillManager::LifecycleState new_state) override;

  // Uses `OnFillOrPreviewFormSentToRenderer()` rather than
  // `OnFillOrPreviewForm()` because the latter may be fired asynchronously
  // (see `features::kAutofillPostTaskAfterFillOrPreviewForm`), whereas tests
  // expect the fill and preview events in the order in which they happened
  // relative to, e.g., hiding the suggestions.
  void OnFillOrPreviewFormSentToRenderer(
      AutofillManager& manager,
      FormGlobalId form_id,
      FieldGlobalId trigger_field_id,
      mojom::ActionPersistence action_persistence,
      const base::flat_set<FieldGlobalId>& sent_field_ids) override;

  void OnSuggestionsShown(AutofillManager& manager,
                          base::span<const Suggestion> suggestions) override;

  void OnSuggestionsHidden(AutofillManager& manager,
                           SuggestionHidingReason reason) override;

  base::ScopedMultiSourceObservation<AutofillManager, AutofillManager::Observer>
      observations_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_BROWSER_AUTOFILL_MANAGER_TEST_DELEGATE_H_
