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
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace autofill {

class AutofillClient;
class AutofillDriver;

class OmniboxAutofillDelegate : public AutofillManager::Observer,
                                public AutofillSuggestionDelegate,
                                public mojom::AutofillVisibilityObserver {
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
  void OnAfterDidAutofillForm(AutofillManager& manager,
                              FormGlobalId form) override;

  // AutofillSuggestionDelegate:
  bool OnFilterChanged(const std::u16string& filter) override;
  bool OnSearchSubmitted(const std::u16string& filter) override;
  bool IsSearching() const override;
  std::variant<AutofillDriver*, password_manager::PasswordManagerDriver*>
  GetDriver_DoNotUse() override;
  void OnSuggestionsShown(base::span<const Suggestion> suggestions,
                          const SuggestionUiMetadata& metadata) override;
  void OnSuggestionsHidden(SuggestionHidingReason reason) override;
  void DidSelectSuggestion(const Suggestion& suggestion) override;
  void DidAcceptSuggestion(const Suggestion& suggestion,
                           const SuggestionMetadata& metadata) override;
  bool RemoveSuggestion(const Suggestion& suggestion) override;
  void ClearPreviewedForm() override;
  FillingProduct GetMainFillingProduct() const override;
  void OnTabSelected(TabbedPaneTabType tab_type) override;
  FieldGlobalId GetQueriedFieldId() const override;

  // mojom::AutofillVisibilityObserver:
  void OnFieldBecameVisible() override;

  // Called when the omnibox chip is actually shown to the user.
  void OnChipShown();

 private:
  // Returns `true` if `manager`'s AutofillDriver is active, has no parent, and
  // is not embedded. Returns `false` otherwise. Most OmniboxAutofillDelegate
  // functionality only wants to run on the outermost, main frame, active BAM.
  bool IsOutermostMainFrameActiveAutofillManager(AutofillManager& manager);

  // Returns `true` if `field` is a credit card number field and is considered
  // visible in the DOM.
  //
  // Note: This refers to static DOM visibility and should not be confused with
  // viewport visibility, which is tracked asynchronously by
  // `ObserveFieldVisibility()` and reported via `OnFieldBecameVisible()`.
  bool IsVisibleCreditCardNumberField(const AutofillField& field) const;

  // Checks if the given `field` is in the main frame.
  bool IsFieldInMainFrame(AutofillManager& manager,
                          const AutofillField& field) const;

  // Checks if `trigger_field_global_id_` is in the frame.
  bool IsTriggerFieldGlobalIdInFrame(AutofillDriver& driver) const;

  // Fills or previews the card associated with the `suggestion`.
  void FillOrPreviewCard(const Suggestion& suggestion,
                         mojom::ActionPersistence action_persistence);

  // Resets the Omnibox Autofill flow. Hides the omnibox chip via the payments
  // autofill client if it was shown (when `field_became_visible_` is `true`).
  // Clears `candidate_form_found_`, `field_became_visible_`,
  // `trigger_form_global_id_`, and `trigger_field_global_id_`.
  void Reset();

  // If true, the OmniboxAutofillDelegate is likely waiting for the user to
  // scroll the candidate form into the viewport, so parsing logic to find
  // candidate forms should no longer be run.
  bool candidate_form_found_ = false;

  // If true, the IntersectionObserver reported that the field became visible.
  bool field_became_visible_ = false;

  // The global ID of the form for which Omnibox Autofill should trigger.
  FormGlobalId trigger_form_global_id_;

  // The global ID of the field on which Omnibox Autofill should trigger. Note
  // that this is ensured to be of type CREDIT_CARD_NUMBER.
  FieldGlobalId trigger_field_global_id_;

  // The AutofillManager that contains `trigger_form_global_id_` and
  // `trigger_field_global_id_`.
  base::WeakPtr<AutofillManager> trigger_autofill_manager_;

  const raw_ref<AutofillClient> client_;

  ScopedAutofillManagersObservation autofill_managers_observation_{this};

  mojo::Receiver<mojom::AutofillVisibilityObserver> visibility_receiver_{this};

  base::WeakPtrFactory<OmniboxAutofillDelegate> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_
