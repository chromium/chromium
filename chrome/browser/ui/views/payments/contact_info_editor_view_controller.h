// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_CONTACT_INFO_EDITOR_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_CONTACT_INFO_EDITOR_VIEW_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/payments/editor_view_controller.h"
#include "chrome/browser/ui/views/payments/validation_delegate.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace payments {

class ContactInfoEditorViewController : public EditorViewController {
 public:
  // Does not take ownership of the arguments, which should outlive this object.
  // Passing nullptr as |profile| indicates that we are editing a new profile;
  // other arguments should never be null.
  ContactInfoEditorViewController(
      base::WeakPtr<PaymentRequestSpec> spec,
      base::WeakPtr<PaymentRequestState> state,
      base::WeakPtr<PaymentRequestDialogView> dialog,
      BackNavigationType back_navigation_type,
      base::OnceClosure on_edited,
      base::OnceCallback<void(const autofill::AutofillProfile&)> on_added,
      autofill::AutofillProfile* profile,
      bool is_incognito);
  ~ContactInfoEditorViewController() override;

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

 protected:
  // PaymentRequestSheetController:
  base::string16 GetSheetTitle() override;

 private:
  // Uses the values in the UI fields to populate the corresponding values in
  // |profile|.
  void PopulateProfile(autofill::AutofillProfile* profile);
  bool GetSheetId(DialogViewID* sheet_id) override;
  base::string16 GetValueForType(const autofill::AutofillProfile& profile,
                                 autofill::ServerFieldType type);

  autofill::AutofillProfile* profile_to_edit_;

  // Called when |profile_to_edit_| was successfully edited.
  base::OnceClosure on_edited_;
  // Called when a new profile was added. The const reference is short-lived,
  // and the callee should make a copy.
  base::OnceCallback<void(const autofill::AutofillProfile&)> on_added_;

  class ContactInfoValidationDelegate : public ValidationDelegate {
   public:
    ContactInfoValidationDelegate(const EditorField& field,
                                  const std::string& locale,
                                  ContactInfoEditorViewController* controller);
    ~ContactInfoValidationDelegate() override;

    // ValidationDelegate:
    bool ShouldFormat() override;
    base::string16 Format(const base::string16& text) override;
    bool IsValidTextfield(views::Textfield* textfield,
                          base::string16* error_message) override;
    bool IsValidCombobox(ValidatingCombobox* combobox,
                         base::string16* error_message) override;
    bool TextfieldValueChanged(views::Textfield* textfield,
                               bool was_blurred) override;
    bool ComboboxValueChanged(ValidatingCombobox* combobox) override;
    void ComboboxModelChanged(ValidatingCombobox* combobox) override {}

   private:
    bool ValidateTextfield(views::Textfield* textfield,
                           base::string16* error_message);

    EditorField field_;
    // Outlives this class. Never null.
    ContactInfoEditorViewController* controller_;
    const std::string& locale_;
  };
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_CONTACT_INFO_EDITOR_VIEW_CONTROLLER_H_
