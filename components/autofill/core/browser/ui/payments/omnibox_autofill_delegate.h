// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillClient;
class AutofillDriver;

class OmniboxAutofillDelegate : public AutofillManager::Observer,
                                public AutofillSuggestionDelegate {
 public:
  using AutofillManager::Observer::OnSuggestionsHidden;
  using AutofillManager::Observer::OnSuggestionsShown;

  explicit OmniboxAutofillDelegate(AutofillClient* autofill_client);

  OmniboxAutofillDelegate(const OmniboxAutofillDelegate&) = delete;
  OmniboxAutofillDelegate& operator=(const OmniboxAutofillDelegate&) = delete;

  ~OmniboxAutofillDelegate() override;

  // AutofillManager::Observer:
  void OnFieldTypesDetermined(AutofillManager& manager,
                              FormGlobalId form,
                              AutofillManager::Observer::FieldTypeSource source,
                              bool small_forms_were_parsed) override;
  void OnAutofillManagerStateChanged(
      AutofillManager& manager,
      AutofillDriver::LifecycleState previous,
      AutofillDriver::LifecycleState current) override;
  void OnAfterFormsSeen(AutofillManager& manager,
                        base::span<const FormGlobalId> updated_forms,
                        base::span<const FormGlobalId> removed_forms) override;

  // AutofillSuggestionDelegate:
  bool OnFilterChanged(const std::u16string& filter) override;
  bool OnSearchSubmitted(const std::u16string& filter) override;
  bool IsSearching() const override;
  std::variant<AutofillDriver*, password_manager::PasswordManagerDriver*>
  GetDriver_DoNotUse() override;
  void OnSuggestionsShown(base::span<const Suggestion> suggestions,
                          base::optional_ref<const SuggestionMetadata>
                              parent_suggestion_metadata) override;
  void OnSuggestionsHidden(SuggestionHidingReason reason) override;
  void DidSelectSuggestion(const Suggestion& suggestion) override;
  void DidAcceptSuggestion(const Suggestion& suggestion,
                           const SuggestionMetadata& metadata) override;
  bool RemoveSuggestion(const Suggestion& suggestion) override;
  void ClearPreviewedForm() override;
  FillingProduct GetMainFillingProduct() const override;
  void OnTabSelected(TabbedPaneTabType tab_type) override;

  void OnGetIntersectionObserverInfo(bool is_visible);

 private:
  // Returns `true` if `manager`'s AutofillDriver is active, has no parent, and
  // is not embedded. Returns `false` otherwise. Most OmniboxAutofillDelegate
  // functionality only wants to run on the outermost, main frame, active BAM.
  bool IsOutermostMainFrameActiveAutofillManager(AutofillManager& manager);

  // Checks if the given `field` is in the main frame.
  bool FieldIsInMainFrame(AutofillManager& manager,
                          const AutofillField& field) const;

  // Resets the delegate's internal state, clearing `candidate_form_found_`
  // `trigger_form_global_id_`, and `trigger_field_global_id_`.
  void Reset();

  // Hides the omnibox autofill chip via the payments autofill client and resets
  // the delegate's internal state. This is called when the autofill manager
  // state transitions away from active or when the triggering form is removed
  // from the DOM.
  void HideOmniboxAutofillChip();

  // If true, the OmniboxAutofillDelegate is likely waiting for the user to
  // scroll the candidate form into the viewport, so parsing logic to find
  // candidate forms should no longer be run.
  bool candidate_form_found_ = false;

  // The global ID of the form for which Omnibox Autofill should trigger.
  FormGlobalId trigger_form_global_id_;

  // The global ID of the field on which Omnibox Autofill should trigger. Note
  // that this is ensured to be of type CREDIT_CARD_NUMBER.
  FieldGlobalId trigger_field_global_id_;

  const raw_ref<AutofillClient> client_;

  ScopedAutofillManagersObservation autofill_managers_observation_{this};

  base::WeakPtrFactory<OmniboxAutofillDelegate> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_
