// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

class AutofillDriver;
class AutofillManager;
class CreditCard;

// TODO(csharp): A lot of the logic in this class is copied from autofillagent.
// Once Autofill is moved out of WebKit this class should be the only home for
// this logic. See http://crbug.com/51644

// Delegate for in-browser Autocomplete and Autofill display and selection.
class AutofillExternalDelegate : public AutofillPopupDelegate {
 public:
  // Creates an AutofillExternalDelegate for the specified AutofillManager and
  // AutofillDriver.
  AutofillExternalDelegate(AutofillManager* manager, AutofillDriver* driver);
  virtual ~AutofillExternalDelegate();

  // AutofillPopupDelegate implementation.
  void OnPopupShown() override;
  void OnPopupHidden() override;
  void OnPopupSuppressed() override;
  void DidSelectSuggestion(const base::string16& value,
                           int identifier) override;
  void DidAcceptSuggestion(const base::string16& value,
                           int identifier,
                           int position) override;
  bool GetDeletionConfirmationText(const base::string16& value,
                                   int identifier,
                                   base::string16* title,
                                   base::string16* body) override;
  bool RemoveSuggestion(const base::string16& value, int identifier) override;
  void ClearPreviewedForm() override;

  // Returns PopupType::kUnspecified for all popups prior to |onQuery|, or the
  // popup type after call to |onQuery|.
  PopupType GetPopupType() const override;

  AutofillDriver* GetAutofillDriver() override;

  // Returns the ax node id associated with the current web contents' element
  // who has a controller relation to the current autofill popup.
  int32_t GetWebContentsPopupControllerAxId() const override;

  void RegisterDeletionCallback(base::OnceClosure deletion_callback) override;

  // Records and associates a query_id with web form data.  Called
  // when the renderer posts an Autofill query to the browser. |bounds|
  // is window relative. We might not want to display the warning if a website
  // has disabled Autocomplete because they have their own popup, and showing
  // our popup on to of theirs would be a poor user experience.
  virtual void OnQuery(int query_id,
                       const FormData& form,
                       const FormFieldData& field,
                       const gfx::RectF& element_bounds);

  // Records query results and correctly formats them before sending them off
  // to be displayed.  Called when an Autofill query result is available.
  virtual void OnSuggestionsReturned(int query_id,
                                     const std::vector<Suggestion>& suggestions,
                                     bool autoselect_first_suggestion,
                                     bool is_all_server_suggestions = false);

  // Returns true if there is a screen reader installed on the machine.
  virtual bool HasActiveScreenReader() const;

  // Indicates on focus changed if autofill/autocomplete is available or
  // unavailable, so state can be announced by screen readers.
  virtual void OnAutofillAvailabilityEvent(const mojom::AutofillState state);

  // Set the data list value associated with the current field.
  void SetCurrentDataListValues(
      const std::vector<base::string16>& data_list_values,
      const std::vector<base::string16>& data_list_labels);

  // Inform the delegate that the text field editing has ended. This is
  // used to help record the metrics of when a new popup is shown.
  void DidEndTextFieldEditing();

  // Returns the delegate to its starting state by removing any page specific
  // values or settings.
  void Reset();

  const FormData& query_form() const { return query_form_; }

 protected:
  base::WeakPtr<AutofillExternalDelegate> GetWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillExternalDelegateUnitTest,
                           FillCreditCardForm);

  // Called when a credit card is scanned using device camera.
  void OnCreditCardScanned(const CreditCard& card);

  // Fills the form with the Autofill data corresponding to |unique_id|.
  // If |is_preview| is true then this is just a preview to show the user what
  // would be selected and if |is_preview| is false then the user has selected
  // this data.
  void FillAutofillFormData(int unique_id, bool is_preview);

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

  // Returns the text (i.e. |Suggestion| value) for Chrome autofill options.
  base::string16 GetSettingsSuggestionValue() const;

  AutofillManager* const manager_;  // weak.

  // Provides driver-level context to the shared code of the component. Must
  // outlive this object.
  AutofillDriver* const driver_;  // weak

  // The ID of the last request sent for form field Autofill.  Used to ignore
  // out of date responses.
  int query_id_ = 0;

  // The current form and field selected by Autofill.
  FormData query_form_;
  FormFieldData query_field_;

  // The bounds of the form field that user is interacting with.
  gfx::RectF element_bounds_;

  // Does the popup include any Autofill profile or credit card suggestions?
  bool has_autofill_suggestions_ = false;

  bool should_show_scan_credit_card_ = false;
  PopupType popup_type_ = PopupType::kUnspecified;

  // Whether the credit card signin promo should be shown to the user.
  bool should_show_cc_signin_promo_ = false;

  bool should_show_cards_from_account_option_ = false;

  // The current data list values.
  std::vector<base::string16> data_list_values_;
  std::vector<base::string16> data_list_labels_;

  // If not null then it will be called in destructor.
  base::OnceClosure deletion_callback_;

  base::WeakPtrFactory<AutofillExternalDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegate);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_
