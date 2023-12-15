// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_CONTACT_INFO_EDITOR_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_CONTACT_INFO_EDITOR_VIEW_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
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
  std::u16string GetInitialValueForType(autofill::FieldType type) override;
  bool ValidateModelAndSave() override;
  std::unique_ptr<ValidationDelegate> CreateValidationDelegate(
      const EditorField& field) override;
  std::unique_ptr<ui::ComboboxModel> GetComboboxModelForType(
      const autofill::FieldType& type) override;

 protected:
  // PaymentRequestSheetController:
  std::u16string GetSheetTitle() override;
  base::WeakPtr<PaymentRequestSheetController> GetWeakPtr() override;

 private:
  // Uses the values in the UI fields to populate the corresponding values in
  // |profile|.
  void PopulateProfile(autofill::AutofillProfile* profile);
  bool GetSheetId(DialogViewID* sheet_id) override;
  std::u16string GetValueForType(const autofill::AutofillProfile& profile,
                                 autofill::FieldType type);

  raw_ptr<autofill::AutofillProfile> profile_to_edit_;

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
    std::u16string Format(const std::u16string& text) override;
    bool IsValidTextfield(views::Textfield* textfield,
                          std::u16string* error_message) override;
    bool IsValidCombobox(ValidatingCombobox* combobox,
                         std::u16string* error_message) override;
    bool TextfieldValueChanged(views::Textfield* textfield,
                               bool was_blurred) override;
    bool ComboboxValueChanged(ValidatingCombobox* combobox) override;
    void ComboboxModelChanged(ValidatingCombobox* combobox) override {}

   private:
    bool ValidateTextfield(views::Textfield* textfield,
                           std::u16string* error_message);

    EditorField field_;
    // Outlives this class. Never null.
    raw_ptr<ContactInfoEditorViewController, DanglingUntriaged> controller_;
    const raw_ref<const std::string> locale_;
  };

  // Must be the last member of a leaf class.
  base::WeakPtrFactory<ContactInfoEditorViewController> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_CONTACT_INFO_EDITOR_VIEW_CONTROLLER_H_
