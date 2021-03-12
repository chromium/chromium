// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_EDITOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_EDITOR_VIEW_H_

#include <unordered_map>

#include "chrome/browser/ui/autofill/address_editor_controller.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "ui/views/view.h"

namespace views {
class GridLayout;
class Combobox;
class Textfield;
class View;
}  // namespace views

class AddressEditorController;

namespace autofill {

class AddressEditorView : public views::View {
 public:
  using TextFieldsMap =
      std::unordered_map<views::Textfield*, const EditorField>;
  using ComboboxMap = std::unordered_map<views::Combobox*, const EditorField>;

  explicit AddressEditorView(AddressEditorController* controller);
  AddressEditorView(const AddressEditorView&) = delete;
  AddressEditorView& operator=(const AddressEditorView&) = delete;
  ~AddressEditorView() override;

 private:
  const ComboboxMap& comboboxes() const { return comboboxes_; }
  const TextFieldsMap& text_fields() const { return text_fields_; }

  // Creates the whole editor view to go within the editor dialog. It
  // encompasses all the input fields created by CreateInputField().
  void CreateEditorView();

  // Adds some views to |layout|, to represent an input field and its labels.
  // |field| is the field definition, which contains the label and the hint
  // about the length of the input field. Returns the input view for this field.
  views::View* CreateInputField(views::GridLayout* layout,
                                const EditorField& field);

  // Will create a combobox according to the |field| definition. Will also keep
  // track of this field to populate the edited model on save.
  std::unique_ptr<views::Combobox> CreateComboboxForField(
      const EditorField& field);

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

  // Used to remember the association between the input field UI element and
  // the original field definition. The Textfield* and Combobox* are owned
  // by their parent view, this only keeps a reference that is good as long
  // as the input field is visible.
  TextFieldsMap text_fields_;
  ComboboxMap comboboxes_;
  const std::string locale_;
  AddressEditorController* controller_;

  base::WeakPtrFactory<AddressEditorView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_EDITOR_VIEW_H_
