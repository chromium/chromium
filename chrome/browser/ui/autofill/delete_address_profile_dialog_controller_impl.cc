// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/delete_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/delete_address_profile_dialog_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "delete_address_profile_dialog_controller_impl.h"
#include "ui/views/widget/widget.h"

// TODO(crbug.com/1459990): Implement missing methods.
namespace autofill {

DeleteAddressProfileDialogControllerImpl::
    DeleteAddressProfileDialogControllerImpl(content::WebContents* web_contents)
    : content::WebContentsUserData<DeleteAddressProfileDialogControllerImpl>(
          *web_contents),
      web_contents_(web_contents) {}

// We do not need to close the widget because both the widget and the controller
// are bound to the Webcontents. Therefore if the controller is destroyed, so
// is the widget.
DeleteAddressProfileDialogControllerImpl::
    ~DeleteAddressProfileDialogControllerImpl() = default;

void DeleteAddressProfileDialogControllerImpl::OfferDelete() {
  if (!widget_dialog_) {
    widget_dialog_ = dialogs::ShowDeleteAddressProfileDialogView(web_contents_,
                                                                 GetWeakPtr());
  }
}

std::u16string DeleteAddressProfileDialogControllerImpl::GetAccount() const {
  return std::u16string();
}

void DeleteAddressProfileDialogControllerImpl::OnAccepted() {}

void DeleteAddressProfileDialogControllerImpl::OnCanceled() {}

void DeleteAddressProfileDialogControllerImpl::OnClosed() {}

void DeleteAddressProfileDialogControllerImpl::OnDialogDestroying() {
  widget_dialog_ = nullptr;
}

base::WeakPtr<DeleteAddressProfileDialogController>
DeleteAddressProfileDialogControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DeleteAddressProfileDialogControllerImpl);

}  // namespace autofill
