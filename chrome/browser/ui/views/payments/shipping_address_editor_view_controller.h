// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_ADDRESS_EDITOR_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_ADDRESS_EDITOR_VIEW_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/payments/editor_view_controller.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "components/autofill/core/browser/ui/region_combobox_model.h"

namespace autofill {
class AutofillProfile;
class CountryComboboxModel;
}  // namespace autofill

namespace payments {

class PaymentRequestSpec;
class PaymentRequestState;
class PaymentRequestDialogView;

// Shipping Address editor screen of the Payment Request flow.
class ShippingAddressEditorViewController : public EditorViewController {
 public:
  // Does not take ownership of the arguments (except for the |on_edited| and
  // |on_added| callbacks), which should outlive this object. Additionally,
  // |profile| could be nullptr if we are adding a new shipping address. Else,
  // it's a valid pointer to a card that needs to be updated, and which will
  // outlive this controller.
  ShippingAddressEditorViewController(
      PaymentRequestSpec* spec,
      PaymentRequestState* state,
      PaymentRequestDialogView* dialog,
      BackNavigationType back_navigation_type,
      base::OnceClosure on_edited,
      base::OnceCallback<void(const autofill::AutofillProfile&)> on_added,
      autofill::AutofillProfile* profile,
      bool is_incognito);
  ~ShippingAddressEditorViewController() override;

  // EditorViewController:
  bool IsEditingExistingItem() override;
  std::vector<EditorField> GetFieldDefinitions() override;
  base::string16 GetInitialValueForType(
      autofill::ServerFieldType type) override;
  bool ValidateModelAndSave() override;
  std::unique_ptr<ValidationDelegate> CreateValidationDelegate(
      const EditorField& field) override;
  std::unique_ptr<ui::ComboboxModel> GetComboboxModelForType(
      const autofill::ServerFieldType& type) override;
  void OnPerformAction(views::Combobox* combobox) override;
  void UpdateEditorView() override;

  // PaymentRequestSheetController:
  base::string16 GetSheetTitle() override;
  std::unique_ptr<views::Button> CreatePrimaryButton() override;

 private:
  friend class ShippingAddressValidationDelegate;
  class ShippingAddressValidationDelegate : public ValidationDelegate {
   public:
    ShippingAddressValidationDelegate(
        ShippingAddressEditorViewController* parent,
        const EditorField& field);
    ~ShippingAddressValidationDelegate() override;

    // ValidationDelegate:
    bool ShouldFormat() override;
    base::string16 Format(const base::string16& text) override;
    bool IsValidTextfield(views::Textfield* textfield,
                          base::string16* error_message) override;
    bool IsValidCombobox(views::Combobox* combobox,
                         base::string16* error_message) override;
    bool TextfieldValueChanged(views::Textfield* textfield,
                               bool was_blurred) override;
    bool ComboboxValueChanged(views::Combobox* combobox) override;
    void ComboboxModelChanged(views::Combobox* combobox) override;

   private:
    bool ValidateValue(const base::string16& value,
                       base::string16* error_message);

    EditorField field_;

    // Raw pointer back to the owner of this class, therefore will not be null.
    ShippingAddressEditorViewController* controller_;

    DISALLOW_COPY_AND_ASSIGN(ShippingAddressValidationDelegate);
  };

  base::string16 GetValueForType(const autofill::AutofillProfile& profile,
                                 autofill::ServerFieldType type);

  bool GetSheetId(DialogViewID* sheet_id) override;

  // Updates |countries_| with the content of |model| if it's not null,
  // otherwise use a local model.
  void UpdateCountries(autofill::CountryComboboxModel* model);

  // Updates |editor_fields_| based on the current country.
  void UpdateEditorFields();

  // Called when data changes need to force a view update. |synchronous|
  // specifies whether the view update can be done synchronously.
  void OnDataChanged(bool synchronous);

  // Saves the current state of the |editor_fields_| in |profile| and ignore
  // errors if |ignore_errors| is true. Return false on errors, ignored or not.
  bool SaveFieldsToProfile(autofill::AutofillProfile* profile,
                           bool ignore_errors);

  // When a combobox model has changed, a view update might be needed, e.g., if
  // there is no data in the combobox and it must be converted to a text field.
  void OnComboboxModelChanged(views::Combobox* combobox);

  // Called when |profile_to_edit_| was successfully edited.
  base::OnceClosure on_edited_;
  // Called when a new profile was added. The const reference is short-lived,
  // and the callee should make a copy.
  base::OnceCallback<void(const autofill::AutofillProfile&)> on_added_;

  // If non-nullptr, a point to an object to be edited, which should outlive
  // this controller.
  autofill::AutofillProfile* profile_to_edit_;

  // A temporary profile to keep unsaved data in between relayout (e.g., when
  // the country is changed and fields set may be different).
  autofill::AutofillProfile temporary_profile_;

  // List of fields, reset everytime the current country changes.
  std::vector<EditorField> editor_fields_;

  // The language code to be used for this address, reset everytime the current
  // country changes.
  std::string language_code_;

  // The currently chosen country. Defaults to an invalid constant until
  // |countries_| is properly initialized and then 0 as the first entry in
  // |countries_|, which is the generated default value received from
  // autofill::CountryComboboxModel::countries() which is documented to always
  // have the default country at the top as well as within the sorted list. If
  // |profile_to_edit_| is not null, then use the country from there to set
  // |chosen_country_index_|.
  size_t chosen_country_index_;

  // The list of country codes and names as ordered in the country combobox
  // model.
  std::vector<std::pair<std::string, base::string16>> countries_;

  // Identifies whether we tried and failed to load region data.
  bool failed_to_load_region_data_;

  // Owned by the state combobox, which is owned by this object's base class.
  autofill::RegionComboboxModel* region_model_;

  base::WeakPtrFactory<ShippingAddressEditorViewController> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ShippingAddressEditorViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_ADDRESS_EDITOR_VIEW_CONTROLLER_H_
