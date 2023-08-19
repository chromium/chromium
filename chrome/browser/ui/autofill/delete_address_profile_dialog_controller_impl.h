// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/delete_address_profile_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace views {
class Widget;
}  // namespace views

namespace autofill {

class DeleteAddressProfileDialogControllerImpl
    : DeleteAddressProfileDialogController,
      public content::WebContentsUserData<
          DeleteAddressProfileDialogControllerImpl> {
 public:
  DeleteAddressProfileDialogControllerImpl(
      const DeleteAddressProfileDialogControllerImpl&) = delete;
  DeleteAddressProfileDialogControllerImpl& operator=(
      const DeleteAddressProfileDialogControllerImpl&) = delete;
  ~DeleteAddressProfileDialogControllerImpl() override;

  void OfferDelete();
  // DeleteAddressProfileDialogController
  std::u16string GetAccount() const override;
  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;
  void OnDialogDestroying() override;

 private:
  explicit DeleteAddressProfileDialogControllerImpl(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      DeleteAddressProfileDialogControllerImpl>;

  base::WeakPtr<DeleteAddressProfileDialogController> GetWeakPtr();

  const raw_ptr<content::WebContents> web_contents_;
  raw_ptr<const views::Widget> widget_dialog_ = nullptr;

  base::WeakPtrFactory<DeleteAddressProfileDialogController> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_DELETE_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_
