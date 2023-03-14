// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_EDITOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_EDITOR_VIEW_H_

#include <memory>
#include <unordered_map>

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

class AddressEditorController;

namespace autofill {

class AddressEditorView : public views::View {
 public:
  METADATA_HEADER(AddressEditorView);
  explicit AddressEditorView(
      std::unique_ptr<AddressEditorController> controller);
  AddressEditorView(const AddressEditorView&) = delete;
  AddressEditorView& operator=(const AddressEditorView&) = delete;
  ~AddressEditorView() override;

  // views::View
  void PreferredSizeChanged() override;

  // Stores the current state of the address profile in the controller, and
  // returns it.
  const autofill::AutofillProfile& GetAddressProfile();

  void SetCountryCodeForTesting(const std::string& code);
  void SetTextInputFieldValueForTesting(autofill::ServerFieldType type,
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

  // Combobox callback.
  void OnPerformAction(views::Combobox* combobox);

  // Called when data changes need to force a view update. The view is updated
  // synchronously.
  void OnDataChanged();

  // Checks all fields and updates their visual status accordingly.
  void Validate();

  std::unique_ptr<AddressEditorController> controller_;

  // Map from TextField to the object that describes it
  std::unordered_map<views::Textfield*, const EditorField> text_fields_;
  const std::string locale_;
  views::Label* validation_error_ = nullptr;

  // 1 subscription to text changes per field.
  std::vector<base::CallbackListSubscription> field_change_callbacks_;

  base::WeakPtrFactory<AddressEditorView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_EDITOR_VIEW_H_
