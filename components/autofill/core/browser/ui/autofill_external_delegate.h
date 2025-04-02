// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_EXTERNAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_EXTERNAL_DELEGATE_H_

#include <string>
#include <variant>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace autofill {

class AutofillDriver;
class BrowserAutofillManager;
class CreditCard;

// Delegate for in-browser Autocomplete and Autofill display and selection.
class AutofillExternalDelegate : public AutofillSuggestionDelegate {
 public:
  class ScopedSuggestionSelectionShortcut;
  using UpdateSuggestionsCallback =
      base::RepeatingCallback<void(std::vector<Suggestion>,
                                   AutofillSuggestionTriggerSource)>;

  // Creates an AutofillExternalDelegate for the specified
  // BrowserAutofillManager and AutofillDriver.
  explicit AutofillExternalDelegate(BrowserAutofillManager* manager);

  AutofillExternalDelegate(const AutofillExternalDelegate&) = delete;
  AutofillExternalDelegate& operator=(const AutofillExternalDelegate&) = delete;

  ~AutofillExternalDelegate() override;

  // Returns true if `item_id` identifies a suggestion which can appear on the
  // first layer of the Autofill popup and can fill form fields.
  static bool IsAutofillAndFirstLayerSuggestionId(SuggestionType item_id);

  // AutofillSuggestionDelegate implementation.
  std::variant<AutofillDriver*, password_manager::PasswordManagerDriver*>
  GetDriver() override;
  void OnSuggestionsShown(base::span<const Suggestion> suggestions) override;
  void OnSuggestionsHidden() override;
  void DidSelectSuggestion(const Suggestion& suggestion) override;
  void DidAcceptSuggestion(const Suggestion& suggestion,
                           const SuggestionMetadata& metadata) override;
  void DidPerformButtonActionForSuggestion(
      const Suggestion& suggestion,
      const SuggestionButtonAction& button_action) override;
  bool RemoveSuggestion(const Suggestion& suggestion) override;
  void ClearPreviewedForm() override;

  // Returns FillingProduct::kNone for all popups prior to
  // `OnSuggestionsReturned`. Returns the filling product of the first
  // suggestion that has a filling product that is not none.
  FillingProduct GetMainFillingProduct() const override;

  // Called when the renderer posts an Autofill query to the browser. We might
  // not want to display the warning if a website has disabled Autocomplete
  // because they have their own popup, and showing our popup on to of theirs
  // would be a poor user experience.
  // `caret_bounds` represents the position of the focused field caret. This is
  // used as bounds to anchor the Autofill popup on. Today this is only used by
  // compose suggestions.
  //
  // TODO(crbug.com/40144964): Storing `form` and `field` in member variables
  // breaks the cache.
  virtual void OnQuery(const FormData& form,
                       const FormFieldData& field,
                       const gfx::Rect& caret_bounds,
                       AutofillSuggestionTriggerSource trigger_source,
                       bool update_datalist);

  // Records query results and correctly formats them before sending them off
  // to be displayed. Called when an Autofill query result is available.
  // `suggestion_ranking_context` contains information regarding the ranking of
  // suggestions in `input_suggestions` and is used for metrics logging.
  virtual void OnSuggestionsReturned(
      FieldGlobalId field_id,
      const std::vector<Suggestion>& input_suggestions,
      std::optional<autofill_metrics::SuggestionRankingContext>
          suggestion_ranking_context);

  // Returns true if there is a screen reader installed on the machine.
  virtual bool HasActiveScreenReader() const;

  // Indicates on focus changed if autofill/autocomplete is available or
  // unavailable, so `suggestion_availability` can be announced by screen
  // readers.
  virtual void OnAutofillAvailabilityEvent(
      mojom::AutofillSuggestionAvailability suggestion_availability);

  // Informs the delegate that the text field editing has ended. This is
  // used to help record the metrics of when a new popup is shown.
  void DidEndTextFieldEditing();

  const FormData& query_form() const { return query_form_; }

  void AttemptToDisplayAutofillSuggestionsForTest(
      std::vector<Suggestion> suggestions,
      std::optional<autofill_metrics::SuggestionRankingContext>
          suggestion_ranking_context,
      AutofillSuggestionTriggerSource trigger_source,
      bool is_update) {
    AttemptToDisplayAutofillSuggestions(std::move(suggestions),
                                        std::move(suggestion_ranking_context),
                                        trigger_source, is_update);
  }
  base::WeakPtr<AutofillExternalDelegate> GetWeakPtrForTest() {
    return GetWeakPtr();
  }

 private:
  // Tries to display `suggestions` in the suggestions UI. If `is_update` is
  // true, then `AutofillClient::UpdateAutofillSuggestions` is called, which
  // means that suggestions will only be shown if there is currently suggestion
  // UI with the same main filling product showing and that no new
  // `SuggestionsUiSessionId` will be assigned.
  void AttemptToDisplayAutofillSuggestions(
      std::vector<Suggestion> suggestions,
      std::optional<autofill_metrics::SuggestionRankingContext>
          suggestion_ranking_context,
      AutofillSuggestionTriggerSource trigger_source,
      bool is_update);

  // Returns a callback that, when run, attempts to update the currently shown
  // suggestions. If the `SuggestionUiSessionId` of the currently showing UI
  // surface has changed between when this callback is created and when it is
  // run, running it is a no-op. The callback is also safe to call even if
  // `this` is no longer alive.
  base::RepeatingCallback<void(std::vector<Suggestion>,
                               AutofillSuggestionTriggerSource)>
  CreateUpdateSuggestionsCallback();

  // Returns a callback that, when run, attempts to close the currently shown
  // suggestion UI. If the `SuggestionUiSessionId` of the currently showing UI
  // surface has changed between when this callback is created and when it is
  // run, running it is a no-op. The callback is also safe to call even if
  // `this` is no longer alive.
  base::OnceCallback<void(SuggestionHidingReason)>
  CreateHideSuggestionsCallback();

  // Creates a callback that, when run, fills the field that was last queried
  // when the callback was created.
  base::RepeatingCallback<void(const std::u16string&)>
  CreateSingleFieldFillCallback(SuggestionType suggestion_type,
                                std::optional<FieldType> field_type_used);

  // Private handler for DidAcceptSuggestions for address related suggestions.
  void DidAcceptAddressSuggestion(const Suggestion& suggestion,
                                  const SuggestionMetadata& metadata);

  // Private handler for DidAcceptSuggestions for payments related suggestions.
  void DidAcceptPaymentsSuggestion(const Suggestion& suggestion,
                                   const SuggestionMetadata& metadata);

  // Creates a specialized version of a single field fill callback that converts
  // the argument from UTF8 to UTF16 and set `EMAIL_ADDRESS` as the filled type.
  PlusAddressCallback CreatePlusAddressCallback(SuggestionType suggestion_type);

  // Creates a plus address callback (see `CreatePlusAddressCallback`) which
  // triggers a plus address was created using the manual fallback.
  PlusAddressCallback CreateInlinePlusAddressCallback(
      SuggestionType suggestion_type);

  // Informs the `AutofillPlusAddress` delegate and passes callbacks for
  // hiding/updating suggestions UI and filling.
  void DidAcceptCreateNewPlusAddressInlineSuggestion(
      const Suggestion& suggestion);

  // Called when a credit card is scanned using device camera.
  void OnCreditCardScanned(const CreditCard& card);

  // Returns the last Autofill triggering field. Derived from the `form` and
  // `field` parameters of `OnQuery(). Returns nullptr if called before
  // `OnQuery()` or if the `form` becomes outdated, see crbug.com/1117028.
  const AutofillField* GetQueriedAutofillField() const;

  // Fills the form with the Autofill data corresponding to `guid`.
  // If `is_preview` is true then this is just a preview to show the user what
  // would be selected and if `is_preview` is false then the user has selected
  // this data.
  void FillAutofillFormData(SuggestionType type,
                            const Suggestion::Payload& payload,
                            std::optional<SuggestionMetadata> metadata,
                            bool is_preview,
                            AutofillTriggerSource trigger_source);

  // Previews the value from `profile` specified in the `suggestion`.
  void PreviewAddressFieldByFieldFillingSuggestion(
      const AutofillProfile& profile,
      const Suggestion& suggestion);

  // Fills the value from `profile` specified in the `suggestion`. Emits
  // necessary metrics based on the
  // `suggestion.field_by_field_filling_type_used`.
  void FillAddressFieldByFieldFillingSuggestion(
      const AutofillProfile& profile,
      const Suggestion& suggestion,
      const SuggestionMetadata& metadata);

  // Handle applying any Autofill option listings to the Autofill popup.
  // This function should only get called when there is at least one
  // multi-field suggestion in the list of suggestions.
  // |is_all_server_suggestions| should be true if |suggestions| are empty or
  // all |suggestions| come from Google Payments.
  void ApplyAutofillOptions(std::vector<Suggestion>* suggestions,
                            bool is_all_server_suggestions);

  // Inserts the data list values at the start of the given list, including
  // any required separators. Will also go through `suggestions` and remove
  // duplicate autocomplete (not Autofill) suggestions, keeping their datalist
  // version.
  void InsertDataListValues(std::vector<Suggestion>& suggestions) const;

  // Returns the text (i.e. |Suggestion| value) for Chrome autofill options.
  std::u16string GetSettingsSuggestionValue() const;

  // Returns the trigger source to use to reopen the popup after an edit or
  // delete address profile dialog is closed.
  AutofillSuggestionTriggerSource GetReopenTriggerSource() const;

  // Checks the user's accepted suggestion and logs metrics on the ranking of
  // the suggestion in the Autofill dropdown.
  void LogRankingContextAfterSuggestionAccepted(
      const Suggestion& accepted_suggestion);

  base::WeakPtr<AutofillExternalDelegate> GetWeakPtr();

  // If non-negative, OnSuggestionsReturned() passes one of the suggestions
  // directly to DidAcceptSuggestion(). See ScopedSuggestionSelectionShortcut
  // for details.
  static int shortcut_test_suggestion_index_;

  const raw_ref<BrowserAutofillManager> manager_;

  // The current form and field selected by Autofill.
  FormData query_form_;
  FormFieldData query_field_;
  // The method how suggestions were triggered on the current form.
  AutofillSuggestionTriggerSource trigger_source_;

  std::vector<SuggestionType> shown_suggestion_types_;

  // Contains information on the ranking of suggestions using the new and old
  // ranking algorithm. Used for metrics logging. If the new ranking algorithm
  // is not enabled, this will be nullopt.
  std::optional<autofill_metrics::SuggestionRankingContext>
      suggestion_ranking_context_;

  // The current data list values.
  std::vector<SelectOption> datalist_;

  // The caret position of the focused field.
  gfx::Rect caret_bounds_;

  base::WeakPtrFactory<AutofillExternalDelegate> weak_ptr_factory_{this};
};

// When in scope, OnSuggestionsReturned() directly passes one of the Suggestions
// to DidAcceptSuggestion() rather than displaying the Autofill popup.
//
// Specifically, the passed suggestion is the `index`th testing suggestion.
// Testing suggestions come from PersonalDataManager::test_*().
//
// For security reasons, the passed suggestion must correspond to a testing
// profile from PersonalDataManager. This is asserted by a CHECK(). The CHECK()
// also fails if no `index`th test suggestion exists.
//
// Typical usage is as a member of a test fixture. It can also be used at a
// narrower scope around, for example, AutofillDriver::AskForValuesToFill(),
// but beware of potential asynchronicity (e.g., due to asynchronous parsing or
// asynchronous fetching of suggestions).
class AutofillExternalDelegate::ScopedSuggestionSelectionShortcut {
 public:
  explicit ScopedSuggestionSelectionShortcut(int index = 0) {
    DCHECK(index >= 0);
    DCHECK(shortcut_test_suggestion_index_ < 0);
    shortcut_test_suggestion_index_ = index;
  }

  ScopedSuggestionSelectionShortcut(const ScopedSuggestionSelectionShortcut&) =
      delete;
  ScopedSuggestionSelectionShortcut& operator=(
      const ScopedSuggestionSelectionShortcut&) = delete;

  ~ScopedSuggestionSelectionShortcut() {
    DCHECK(shortcut_test_suggestion_index_ >= 0);
    shortcut_test_suggestion_index_ = -1;
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_EXTERNAL_DELEGATE_H_
