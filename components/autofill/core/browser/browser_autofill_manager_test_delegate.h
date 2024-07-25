// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_DELEGATE_H_

#include "base/scoped_multi_source_observation.h"
#include "components/autofill/core/browser/autofill_manager.h"

namespace autofill {

// Deprecated. Use AutofillManager::Observer instead, especially WaitForEvent()
// instead.
// TODO(crbug.com/40279936): Remove this class.
class BrowserAutofillManagerTestDelegate : public AutofillManager::Observer {
 public:
  BrowserAutofillManagerTestDelegate();
  ~BrowserAutofillManagerTestDelegate() override;

  void Observe(AutofillManager& manager);

  // Called when a form is previewed with Autofill suggestions.
  virtual void DidPreviewFormData() = 0;

  // Called when a form is filled with Autofill suggestions.
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

  void OnFillOrPreviewDataModelForm(
      AutofillManager& manager,
      FormGlobalId form,
      mojom::ActionPersistence action_persistence,
      base::span<const FormFieldData* const> filled_fields,
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card) override;

  void OnSuggestionsShown(AutofillManager& manager) override;

  void OnSuggestionsHidden(AutofillManager& manager) override;

  base::ScopedMultiSourceObservation<AutofillManager, AutofillManager::Observer>
      observations_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_TEST_DELEGATE_H_
