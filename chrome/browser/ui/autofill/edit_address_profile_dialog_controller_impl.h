// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class AutofillBubbleBase;

// The controller functionality for EditAddressProfileView.
class EditAddressProfileDialogControllerImpl
    : public EditAddressProfileDialogController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          EditAddressProfileDialogControllerImpl> {
 public:
  using EditAddressProfileViewTestingFactory =
      base::RepeatingCallback<AutofillBubbleBase*(
          content::WebContents*,
          EditAddressProfileDialogController*)>;

  EditAddressProfileDialogControllerImpl(
      const EditAddressProfileDialogControllerImpl&) = delete;
  EditAddressProfileDialogControllerImpl& operator=(
      const EditAddressProfileDialogControllerImpl&) = delete;
  ~EditAddressProfileDialogControllerImpl() override;

  // Sets up the controller and offers to edit the `profile` before saving it.
  // The dialog title will be set to `title_override` if it's not an empty
  // string. Otherwise, it will default to "Edit address". The footer will only
  // be displayed if the `footer_message` is not an empty string.
  // The `on_user_decision_callback` will be called when user closes the dialog.
  // `is_editing_existing_address` is used for adapting the UI, e.g. "Save" or
  // "Update" for the action button. `is_migration_to_account` is used
  // to determine if a subset of editor fields should be made required.
  void OfferEdit(const AutofillProfile& profile,
                 const std::u16string& title_override,
                 const std::u16string& footer_message,
                 bool is_editing_existing_address,
                 bool is_migration_to_account,
                 AutofillClient::AddressProfileSavePromptCallback
                     on_user_decision_callback);

  // EditAddressProfileDialogController:
  std::u16string GetWindowTitle() const override;
  const std::u16string& GetFooterMessage() const override;
  std::u16string GetOkButtonLabel() const override;
  const AutofillProfile& GetProfileToEdit() const override;
  bool GetIsValidatable() const override;
  void OnDialogClosed(
      AutofillClient::AddressPromptUserDecision decision,
      base::optional_ref<const AutofillProfile> profile_with_edits) override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  void SetViewFactoryForTest(EditAddressProfileViewTestingFactory factory);

 private:
  explicit EditAddressProfileDialogControllerImpl(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      EditAddressProfileDialogControllerImpl>;

  // Remove the |dialog_view_| and hide the dialog.
  void HideDialog();

  // nullptr if no dialog is currently shown.
  raw_ptr<AutofillBubbleBase> dialog_view_ = nullptr;

  // Editor's overridden title, if empty, the default "Edit address" is used.
  std::u16string title_override_;

  // Editor's footnote message.
  std::u16string footer_message_;

  // Callback to run once the user makes a decision with respect to saving the
  // address profile currently being edited.
  AutofillClient::AddressProfileSavePromptCallback on_user_decision_callback_;

  // Contains the details of the address profile that the user requested to edit
  // before saving.
  std::optional<AutofillProfile> address_profile_to_edit_;

  // Whether the address to edit existed and being updated in the editor or
  // the editor is used for creating a new one.
  bool is_editing_existing_address_;

  // Whether the editor is used in the profile migration case. It is required
  // to restore the original prompt state (save or update) if it is reopened.
  bool is_migration_to_account_ = false;

  // Factory used to inject the view instance into this controller in tests.
  EditAddressProfileViewTestingFactory view_factory_for_test_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_
