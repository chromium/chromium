// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/delete_address_profile_dialog_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class DeleteAddressProfileDialogControllerImpl
    : DeleteAddressProfileDialogController,
      public content::WebContentsUserData<
          DeleteAddressProfileDialogControllerImpl> {
 public:
  using DeleteAddressProfileDialogViewFactory = base::RepeatingCallback<void(
      content::WebContents*,
      base::WeakPtr<DeleteAddressProfileDialogController>)>;

  DeleteAddressProfileDialogControllerImpl(
      const DeleteAddressProfileDialogControllerImpl&) = delete;
  DeleteAddressProfileDialogControllerImpl& operator=(
      const DeleteAddressProfileDialogControllerImpl&) = delete;
  ~DeleteAddressProfileDialogControllerImpl() override;

  void OfferDelete(bool is_account_address_profile,
                   AutofillClient::AddressProfileDeleteDialogCallback
                       delete_dialog_callback);
  // DeleteAddressProfileDialogController:
  std::u16string GetTitle() const override;
  std::u16string GetAcceptButtonText() const override;
  std::u16string GetDeclineButtonText() const override;
  std::u16string GetDeleteConfirmationText() const override;

  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;
  void OnDialogDestroying() override;

  void SetViewFactoryForTest(
      DeleteAddressProfileDialogViewFactory view_factory);

 private:
  explicit DeleteAddressProfileDialogControllerImpl(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      DeleteAddressProfileDialogControllerImpl>;

  base::WeakPtr<DeleteAddressProfileDialogController> GetWeakPtr();

  const raw_ptr<content::WebContents> web_contents_;
  bool is_dialog_opened_ = false;
  bool is_account_address_profile_;
  AutofillClient::AddressProfileDeleteDialogCallback delete_dialog_callback_;
  std::optional<bool> user_accepted_;
  DeleteAddressProfileDialogViewFactory view_factory_for_test_;

  base::WeakPtrFactory<DeleteAddressProfileDialogController> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_
