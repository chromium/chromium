// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_

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
  EditAddressProfileDialogControllerImpl(
      const EditAddressProfileDialogControllerImpl&) = delete;
  EditAddressProfileDialogControllerImpl& operator=(
      const EditAddressProfileDialogControllerImpl&) = delete;
  ~EditAddressProfileDialogControllerImpl() override;

  // Sets up the controller and offers to edit the |profile| before saving it.
  // |address_profile_save_prompt_callback| will be invoked once the user makes
  // a decision with respect to the offer-to-edit prompt.
  void OfferEdit(const AutofillProfile& profile,
                 AutofillClient::AddressProfileSavePromptCallback
                     address_profile_save_prompt_callback);

  // EditAddressProfileDialogController:
  std::u16string GetWindowTitle() const override;
  const AutofillProfile& GetProfileToEdit() const override;
  void OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision decision,
      const AutofillProfile& profile_with_edits) override;
  void OnDialogClosed() override;

 private:
  explicit EditAddressProfileDialogControllerImpl(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      EditAddressProfileDialogControllerImpl>;

  // Callback to run once the user makes a decision with respect to saving the
  // address profile currently being edited.
  AutofillClient::AddressProfileSavePromptCallback
      address_profile_save_prompt_callback_;

  // Contains the details of the address profile that the user requested to edit
  // before saving.
  AutofillProfile address_profile_to_edit_;

  AutofillBubbleBase* edit_dialog_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_
