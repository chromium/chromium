// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_EDITOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_EDITOR_VIEW_H_

#include <memory>
#include <unordered_map>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/address_editor_controller.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Combobox;
class Textfield;
class View;
class Label;
}  // namespace views

namespace autofill {

class AddressEditorView : public views::View {
  METADATA_HEADER(AddressEditorView, views::View)

 public:
  explicit AddressEditorView(
      std::unique_ptr<AddressEditorController> controller);
  AddressEditorView(const AddressEditorView&) = delete;
  AddressEditorView& operator=(const AddressEditorView&) = delete;
  ~AddressEditorView() override;

  // The view to be focused first when the editor shows up. It points to
  // the first input (normally the country combobox) and important from
  // the usability perspective as the most reasonable place to start editing
  // the address from.
  // Returns `nullprt` only when `AddressEditorController::editor_fields()`
  // returns no fields.
  views::View* initial_focus_view() { return initial_focus_view_; }

  // views::View
  void PreferredSizeChanged() override;

  // Stores the current state of the address profile in the controller, and
  // returns it.
  // If the editor is validatable (`AddressEditorController::is_validatable()`),
  // make sure the address is valid (`AddressEditorController::is_valid()`)
  // before calling this method.
  const autofill::AutofillProfile& GetAddressProfile();

  // Checks all fields and updates their visual status accordingly. Returns
  // `false` if at least one field is invalid and `true` otherwise or if
  // the form is not validatable.
  bool ValidateAllFields();

  void SelectCountryForTesting(const std::u16string& code);
  void SetTextInputFieldValueForTesting(autofill::FieldType type,
                                        const std::u16string& value);
  std::u16string GetValidationErrorForTesting() const;

 private:
  // Creates the whole editor view to go within the editor dialog. It
  // encompasses all the input fields created by CreateInputField().
  void CreateEditorView();

  // Adds some views to |layout|, to represent an input field and its labels.
  // |field| is the field definition, which contains the label and the hint
  // about the length of the input field. Returns the input view for this field.
  views::View* CreateInputField(const EditorField& field);

  // Will create a country combobox with |label|. Will also keep track of this
  // field to populate the edited model on save.
  std::unique_ptr<views::Combobox> CreateCountryCombobox(
      const std::u16string& label);

  // Update the editor view by removing all it's child views and recreating
  // the input fields using |editor_fields_|. Note that
  // CreateEditorView MUST have been called at least once before calling
  // UpdateEditorView.
  void UpdateEditorView();

  void SaveFieldsToProfile();

  // Combobox callback. Called when data changes need to force a view update.
  // The view is updated synchronously.
  void OnSelectedCountryChanged(views::Combobox* combobox);

  // Checks the field and updates its visual status accordingly.
  void ValidateField(views::Textfield* textfield);

  std::unique_ptr<AddressEditorController> controller_;

  // Map from TextField to the object that describes it.
  std::unordered_map<views::Textfield*, const EditorField> text_fields_;
  const std::string locale_;
  raw_ptr<views::Label> validation_error_ = nullptr;

  // The first input field, depends on the `controller_->editor_fields()`, but
  // normally is the country selection combobox.
  raw_ptr<views::View> initial_focus_view_ = nullptr;

  // The property is set to `true` after the first call of
  // `ValidateAllFields()`. It affects the validation logic upon on a single
  // field change: if it is `false`,only the edited erroneous fields are
  // highlighted, otherwise, the whole form validity is reconsidred. This
  // ensures a user-friendly experience by initially displaying the form as
  // error-free (for new profiles).
  bool all_address_fields_have_been_validated_ = false;

  // 1 subscription to text changes per field.
  std::vector<base::CallbackListSubscription> field_change_callbacks_;

  base::WeakPtrFactory<AddressEditorView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_EDITOR_VIEW_H_
