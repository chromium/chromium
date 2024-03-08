// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

class AutofillDriver;
class BrowserAutofillManager;
class CreditCard;
enum class CreditCardFetchResult;

// Delegate for in-browser Autocomplete and Autofill display and selection.
class AutofillExternalDelegate : public AutofillPopupDelegate,
                                 public PersonalDataManagerObserver {
 public:
  class ScopedAutofillPopupShortcutForTesting;

  // Creates an AutofillExternalDelegate for the specified
  // BrowserAutofillManager and AutofillDriver.
  explicit AutofillExternalDelegate(BrowserAutofillManager* manager);

  AutofillExternalDelegate(const AutofillExternalDelegate&) = delete;
  AutofillExternalDelegate& operator=(const AutofillExternalDelegate&) = delete;

  ~AutofillExternalDelegate() override;

  // Returns true if `item_id` identifies a suggestion which can appear on the
  // first layer of the Autofill popup and can fill form fields.
  static bool IsAutofillAndFirstLayerSuggestionId(PopupItemId item_id);

  // AutofillPopupDelegate implementation.
  absl::variant<AutofillDriver*, password_manager::PasswordManagerDriver*>
  GetDriver() override;
  void OnPopupShown() override;
  void OnPopupHidden() override;
  void DidSelectSuggestion(const Suggestion& suggestion) override;
  void DidAcceptSuggestion(const Suggestion& suggestion,
                           const SuggestionPosition& position) override;
  void DidPerformButtonActionForSuggestion(
      const Suggestion& suggestion) override;
  bool RemoveSuggestion(const Suggestion& suggestion) override;
  void ClearPreviewedForm() override;

  // Returns FillingProduct::kNone for all popups prior to
  // `OnSuggestionsReturned`. Returns the filling product of the first
  // suggestion that has a filling product that is not none.
  FillingProduct GetMainFillingProduct() const override;

  // Called when the renderer posts an Autofill query to the browser. |bounds|
  // is window relative. We might not want to display the warning if a website
  // has disabled Autocomplete because they have their own popup, and showing
  // our popup on to of theirs would be a poor user experience.
  //
  // TODO(crbug.com/1117028): Storing `form` and `field` in member variables
  // breaks the cache.
  virtual void OnQuery(const FormData& form,
                       const FormFieldData& field,
                       const gfx::RectF& element_bounds,
                       AutofillSuggestionTriggerSource trigger_source);

  // Records query results and correctly formats them before sending them off
  // to be displayed. Called when an Autofill query result is available.
  virtual void OnSuggestionsReturned(
      FieldGlobalId field_id,
      const std::vector<Suggestion>& suggestions);

  // Returns the last targeted field types to be filled. This does not
  // equate to the field types that were actually filed, but only to those
  // that were targeted. If a field type is not present on the form that
  // triggered the suggestions, it cannot possibly be filled.
  // This is used by group filling to keep users in the same granularity level
  // by filtering out fields that do not match the last targeted fields group
  // granularity. For example, if users choose to fill every address field, we
  // will store these fields so that in a next iteration, when the user clicks,
  // say a name field only fields that are of group name are filled, therefore
  // staying at a group filling level.
  std::optional<FieldTypeSet> GetLastFieldTypesToFillForSection(
      const Section& section) const;

  // Returns true if there is a screen reader installed on the machine.
  virtual bool HasActiveScreenReader() const;

  // Indicates on focus changed if autofill/autocomplete is available or
  // unavailable, so `suggestion_availability` can be announced by screen
  // readers.
  virtual void OnAutofillAvailabilityEvent(
      mojom::AutofillSuggestionAvailability suggestion_availability);

  // Set the data list value associated with the current field.
  void SetCurrentDataListValues(std::vector<SelectOption> datalist);

  // Inform the delegate that the text field editing has ended. This is
  // used to help record the metrics of when a new popup is shown.
  void DidEndTextFieldEditing();

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  const FormData& query_form() const { return query_form_; }

  base::WeakPtr<AutofillExternalDelegate> GetWeakPtrForTest() {
    return GetWeakPtr();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillExternalDelegateUnitTest,
                           FillCreditCardForm);

  base::WeakPtr<AutofillExternalDelegate> GetWeakPtr();

  // Shows the address editor to the user. The Autofill profile to edit is
  // determined by passed `guid`.
  void ShowEditAddressProfileDialog(const std::string& guid);

  // Shows the delete address profile dialog to the user. The Autofill profile
  // to delete is determined by the passed `guid`.
  void ShowDeleteAddressProfileDialog(const std::string& guid);

  // Triggered when the user closes the address editor dialog.
  void OnAddressEditorClosed(AutofillClient::AddressPromptUserDecision decision,
                             base::optional_ref<const AutofillProfile> profile);

  // Triggered when the user closes the delete address profile dialog.
  void OnDeleteDialogClosed(const std::string& guid, bool user_accepted_delete);

  // Called when a credit card is scanned using device camera.
  void OnCreditCardScanned(const AutofillTriggerSource trigger_source,
                           const CreditCard& card);

  // Returns the last Autofill triggering field. Derived from the `form` and
  // `field` parameters of `OnQuery(). Returns nullptr if called before
  // `OnQuery()` or if the `form` becomes outdated, see crbug.com/1117028.
  const AutofillField* GetQueriedAutofillField() const;

  // Fills the form with the Autofill data corresponding to `backend_id`.
  // If `is_preview` is true then this is just a preview to show the user what
  // would be selected and if `is_preview` is false then the user has selected
  // this data.
  void FillAutofillFormData(PopupItemId popup_item_id,
                            Suggestion::BackendId backend_id,
                            bool is_preview,
                            const AutofillTriggerDetails& trigger_details);

  // Determines the correct data type (`AutofillProfile` or `CreditCard`) to be
  // previewed and previews the corresponding field-by-field filling suggestion.
  void PreviewFieldByFieldFillingSuggestion(const Suggestion& suggestion);

  // Determines the correct data type (`AutofillProfile` or `CreditCard`) to be
  // filled and fills the corresponding field-by-field filling suggestion.
  void FillFieldByFieldFillingSuggestion(
      const Suggestion& suggestion,
      const SuggestionPosition& position,
      AutofillSuggestionTriggerSource trigger_source);

  // Previews the value from `profile` specified in the `suggestion`.
  void PreviewAddressFieldByFieldFillingSuggestion(
      const AutofillProfile& profile,
      const Suggestion& suggestion);

  // Previews the main text from the `suggestion`.
  void PreviewCreditCardFieldByFieldFillingSuggestion(
      const Suggestion& suggestion);

  // Fills the value from `profile` specified in the `suggestion`. Emits
  // necessary metrics based on the
  // `suggestion.field_by_field_filling_type_used`.
  void FillAddressFieldByFieldFillingSuggestion(
      const AutofillProfile& profile,
      const Suggestion& suggestion,
      const SuggestionPosition& position,
      AutofillSuggestionTriggerSource trigger_source);

  // Uses the `credit_card` to optionally fetch the credit card number depending
  // on the `suggestion.field_by_field_filling_type_used`. Fills the fetched
  // credit card number or the `suggestion::main_text`.
  void FillCreditCardFieldByFieldFillingSuggestion(
      const CreditCard& credit_card,
      const Suggestion& suggestion);

  // Triggered when the user closes the authentication flow needed to access
  // the number and cvc of the `credit_card`.
  void OnCreditCardFetched(CreditCardFetchResult result,
                           const CreditCard* credit_card);

  // Triggered when the user completes the authentication flow needed to access
  // virtual credit card details.
  void OnVirtualCreditCardFetched(CreditCardFetchResult result,
                                  const CreditCard* credit_card);

  // Will remove Autofill warnings from |suggestions| if there are also
  // autocomplete entries in the vector. Note: at this point, it is assumed that
  // if there are Autofill warnings, they will be at the head of the vector and
  // any entry that is not an Autofill warning is considered an Autocomplete
  // entry.
  void PossiblyRemoveAutofillWarnings(std::vector<Suggestion>* suggestions);

  // Handle applying any Autofill option listings to the Autofill popup.
  // This function should only get called when there is at least one
  // multi-field suggestion in the list of suggestions.
  // |is_all_server_suggestions| should be true if |suggestions| are empty or
  // all |suggestions| come from Google Payments.
  void ApplyAutofillOptions(std::vector<Suggestion>* suggestions,
                            bool is_all_server_suggestions);

  // Insert the data list values at the start of the given list, including
  // any required separators. Will also go through |suggestions| and remove
  // duplicate autocomplete (not Autofill) suggestions, keeping their datalist
  // version.
  void InsertDataListValues(std::vector<Suggestion>* suggestions);

  bool IsPaymentsManualFallbackOnNonPaymentsField() const;

  // Returns the text (i.e. |Suggestion| value) for Chrome autofill options.
  std::u16string GetSettingsSuggestionValue() const;

  // Returns the trigger source to use to reopen the popup after an edit or
  // delete address profile dialog is closed.
  AutofillSuggestionTriggerSource GetReopenTriggerSource() const;

  // If true, OnSuggestionsReturned() passes one of the suggestions directly to
  // DidAcceptSuggestion(). See ScopedAutofillPopupShortcutForTesting for
  // details.
  static int shortcut_test_suggestion_index_;

  const raw_ref<BrowserAutofillManager> manager_;

  // The current form and field selected by Autofill.
  FormData query_form_;
  FormFieldData query_field_;
  // The bounds of the form field that the user is interacting with.
  gfx::RectF element_bounds_;
  // The method how suggestions were triggered on the current form.
  AutofillSuggestionTriggerSource trigger_source_;

  // Stores the last `AutofillTriggerDetails::field_types_to_fill`.
  // We key this information by form section to guarantee granular filling
  // side effects are specific are not "leaked" to other forms.
  base::flat_map<Section, FieldTypeSet>
      last_field_types_to_fill_for_address_form_section_;

  bool show_cards_from_account_suggestion_was_shown_ = false;

  std::vector<PopupItemId> shown_suggestion_types_;

  // The current data list values.
  std::vector<SelectOption> datalist_;

  // Autofill profile update and deletion are async operations. PDM observer is
  // used to detect when these operations finish. These operations can happen at
  // the same time.
  base::ScopedObservation<PersonalDataManager, PersonalDataManagerObserver>
      pdm_observation_{this};

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
class AutofillExternalDelegate::ScopedAutofillPopupShortcutForTesting {
 public:
  explicit ScopedAutofillPopupShortcutForTesting(int index = 0) {
    DCHECK(index >= 0);
    DCHECK(shortcut_test_suggestion_index_ < 0);
    shortcut_test_suggestion_index_ = index;
  }

  ScopedAutofillPopupShortcutForTesting(
      const ScopedAutofillPopupShortcutForTesting&) = delete;
  ScopedAutofillPopupShortcutForTesting& operator=(
      const ScopedAutofillPopupShortcutForTesting&) = delete;

  ~ScopedAutofillPopupShortcutForTesting() {
    DCHECK(shortcut_test_suggestion_index_ >= 0);
    shortcut_test_suggestion_index_ = -1;
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_
