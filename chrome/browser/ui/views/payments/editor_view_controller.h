// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_EDITOR_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_EDITOR_VIEW_CONTROLLER_H_

#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "chrome/browser/ui/views/payments/validation_delegate.h"
#include "components/autofill/core/browser/field_types.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace ui {
class ComboboxModel;
}

namespace views {
class GridLayout;
class Label;
class Textfield;
class View;
}  // namespace views

namespace payments {

class PaymentRequestSpec;
class PaymentRequestState;
class ValidatingCombobox;
class ValidatingTextfield;

// Field definition for an editor field, used to build the UI.
struct EditorField {
  enum class LengthHint : int { HINT_LONG, HINT_SHORT };
  enum class ControlType : int {
    TEXTFIELD,
    TEXTFIELD_NUMBER,
    COMBOBOX,
    CUSTOMFIELD,
    READONLY_LABEL
  };

  EditorField(autofill::ServerFieldType type,
              base::string16 label,
              LengthHint length_hint,
              bool required,
              ControlType control_type = ControlType::TEXTFIELD)
      : type(type),
        label(std::move(label)),
        length_hint(length_hint),
        required(required),
        control_type(control_type) {}

  // Data type in the field.
  autofill::ServerFieldType type;
  // Label to be shown alongside the field.
  base::string16 label;
  // Hint about the length of this field's contents.
  LengthHint length_hint;
  // Whether the field is required.
  bool required;
  // The control type.
  ControlType control_type;
};

// The PaymentRequestSheetController subtype for the editor screens of the
// Payment Request flow.
class EditorViewController : public PaymentRequestSheetController,
                             public views::TextfieldController,
                             public views::ComboboxListener {
 public:
  using TextFieldsMap =
      std::unordered_map<ValidatingTextfield*, const EditorField>;
  using ComboboxMap =
      std::unordered_map<ValidatingCombobox*, const EditorField>;
  using ErrorLabelMap = std::map<autofill::ServerFieldType, views::View*>;

  // Does not take ownership of the arguments, which should outlive this object.
  // |back_navigation_type| identifies what sort of back navigation should be
  // done when editing is successful. This is independent of the back arrow
  // which always goes back one step.
  EditorViewController(PaymentRequestSpec* spec,
                       PaymentRequestState* state,
                       PaymentRequestDialogView* dialog,
                       BackNavigationType back_navigation_type,
                       bool is_incognito);
  ~EditorViewController() override;

  // Will display |error_message| alongside the input field represented by
  // field |type|.
  void DisplayErrorMessageForField(autofill::ServerFieldType type,
                                   const base::string16& error_message);

  const ComboboxMap& comboboxes() const { return comboboxes_; }
  const TextFieldsMap& text_fields() const { return text_fields_; }

  // Returns the View ID that can be used to lookup the input field for |type|.
  static int GetInputFieldViewId(autofill::ServerFieldType type);

 protected:
  // Create a header view to be inserted before all fields.
  virtual std::unique_ptr<views::View> CreateHeaderView();
  // |focusable_field| is to be set with a pointer to the view that should get
  // default focus within the custom view. |valid| should be set to the initial
  // validity state of the custom view. If a custom view requires model
  // validation, it should be tracked in |text_fields_| or |comboboxes_| (e.g.,
  // by using CreateComboboxForField). |error_message| should be set to the
  // error message for the whole custom view, if applicable. It's possible this
  // message will only be shown in certain circumstances by the
  // EditorViewController.
  virtual std::unique_ptr<views::View> CreateCustomFieldView(
      autofill::ServerFieldType type,
      views::View** focusable_field,
      bool* valid,
      base::string16* error_message);
  // Create an extra view to go to the right of the field with |type|, which
  // can either be a textfield, combobox, or custom view.
  virtual std::unique_ptr<views::View> CreateExtraViewForField(
      autofill::ServerFieldType type);
  // Returns whether the editor is editing an existing item.
  virtual bool IsEditingExistingItem() = 0;
  // Returns the field definitions used to build the UI.
  virtual std::vector<EditorField> GetFieldDefinitions() = 0;
  virtual base::string16 GetInitialValueForType(
      autofill::ServerFieldType type) = 0;
  // Validates the data entered and attempts to save; returns true on success.
  virtual bool ValidateModelAndSave() = 0;

  // Creates a ValidationDelegate which knows how to validate for a given
  // |field| definition.
  virtual std::unique_ptr<ValidationDelegate> CreateValidationDelegate(
      const EditorField& field) = 0;
  virtual std::unique_ptr<ui::ComboboxModel> GetComboboxModelForType(
      const autofill::ServerFieldType& type) = 0;

  // Returns true if all fields are valid.
  bool ValidateInputFields();

  // PaymentRequestSheetController;
  std::unique_ptr<views::Button> CreatePrimaryButton() override;
  bool ShouldShowSecondaryButton() override;
  void FillContentView(views::View* content_view) override;

  // views::ComboboxListener:
  void OnPerformAction(views::Combobox* combobox) override;

  // Update the editor view by removing all it's child views and recreating
  // the input fields returned by GetFieldDefinitions. Note that
  // CreateEditorView MUST have been called at least once before calling
  // UpdateEditorView.
  virtual void UpdateEditorView();

  // PaymentRequestSheetController:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  views::View* GetFirstFocusedView() override;

  // Will create a combobox according to the |field| definition. Will also keep
  // track of this field to populate the edited model on save. Fills
  // |error_message| with an error message about this field's data, if
  // appropriate.
  std::unique_ptr<ValidatingCombobox> CreateComboboxForField(
      const EditorField& field,
      base::string16* error_message);

  bool is_incognito() const { return is_incognito_; }

 private:
  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;

  // Creates the whole editor view to go within the editor dialog. It
  // encompasses all the input fields created by CreateInputField().
  std::unique_ptr<views::View> CreateEditorView();

  // Adds some views to |layout|, to represent an input field and its labels.
  // |field| is the field definition, which contains the label and the hint
  // about the length of the input field. A placeholder error label is also
  // added (see implementation). Returns the input view for this field that
  // could be used as the initial focused and set |valid| with false if the
  // initial value of the field is not valid.
  views::View* CreateInputField(views::GridLayout* layout,
                                const EditorField& field,
                                bool* valid);

  // Returns the widest column width of across all extra views of a certain
  // |size| type.
  int ComputeWidestExtraViewWidth(EditorField::LengthHint size);

  void AddOrUpdateErrorMessageForField(autofill::ServerFieldType type,
                                       const base::string16& error_message);

  // Used to remember the association between the input field UI element and the
  // original field definition. The ValidatingTextfield* and ValidatingCombobox*
  // are owned by their parent view, this only keeps a reference that is good as
  // long as the input field is visible.
  TextFieldsMap text_fields_;
  ComboboxMap comboboxes_;
  // Tracks the relationship between a field and its error label.
  ErrorLabelMap error_labels_;

  // The input field view in the editor used to set the initial focus.
  views::View* initial_focus_field_view_;

  // Identifies where to go back when the editing completes successfully.
  BackNavigationType back_navigation_type_;

  bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(EditorViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_EDITOR_VIEW_CONTROLLER_H_
