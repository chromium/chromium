// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/delete_address_profile_dialog_controller_impl.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/ui/autofill/delete_address_profile_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

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

void DeleteAddressProfileDialogControllerImpl::OfferDelete(
    bool is_account_address_profile,
    AutofillClient::AddressProfileDeleteDialogCallback delete_dialog_callback) {
  if (is_dialog_opened_) {
    return;
  }
  is_account_address_profile_ = is_account_address_profile;
  delete_dialog_callback_ = std::move(delete_dialog_callback);

  if (view_factory_for_test_) {
    view_factory_for_test_.Run(web_contents_, GetWeakPtr());
  } else {
    ShowDeleteAddressProfileDialogView(web_contents_, GetWeakPtr());
  }
  is_dialog_opened_ = true;
}

std::u16string DeleteAddressProfileDialogControllerImpl::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_SETTINGS_ADDRESS_REMOVE_CONFIRMATION_TITLE);
}

std::u16string DeleteAddressProfileDialogControllerImpl::GetAcceptButtonText()
    const {
  return l10n_util::GetStringUTF16(IDS_SETTINGS_ADDRESS_REMOVE);
}

std::u16string DeleteAddressProfileDialogControllerImpl::GetDeclineButtonText()
    const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

std::u16string
DeleteAddressProfileDialogControllerImpl::GetDeleteConfirmationText() const {
  if (is_account_address_profile_) {
    std::optional<AccountInfo> account =
        GetPrimaryAccountInfoFromBrowserContext(
            web_contents_->GetBrowserContext());
    CHECK(account);
    return l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_DELETE_ACCOUNT_ADDRESS_RECORD_TYPE_NOTICE,
        base::UTF8ToUTF16(account->email));
  }

  PersonalDataManager* pdm = PersonalDataManagerFactory::GetForBrowserContext(
      web_contents_->GetBrowserContext());
  return l10n_util::GetStringUTF16(
      pdm->address_data_manager().IsSyncFeatureEnabledForAutofill()
          ? IDS_AUTOFILL_DELETE_SYNC_ADDRESS_RECORD_TYPE_NOTICE
          : IDS_AUTOFILL_DELETE_LOCAL_ADDRESS_RECORD_TYPE_NOTICE);
}

void DeleteAddressProfileDialogControllerImpl::OnAccepted() {
  user_accepted_ = true;
}

void DeleteAddressProfileDialogControllerImpl::OnCanceled() {
  user_accepted_ = false;
}

void DeleteAddressProfileDialogControllerImpl::OnClosed() {
  user_accepted_ = false;
}

void DeleteAddressProfileDialogControllerImpl::OnDialogDestroying() {
  if (user_accepted_) {
    std::move(delete_dialog_callback_).Run(user_accepted_.value());
    user_accepted_.reset();
  }
  is_dialog_opened_ = false;
}

base::WeakPtr<DeleteAddressProfileDialogController>
DeleteAddressProfileDialogControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DeleteAddressProfileDialogControllerImpl::SetViewFactoryForTest(
    DeleteAddressProfileDialogViewFactory view_factory) {
  view_factory_for_test_ = view_factory;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DeleteAddressProfileDialogControllerImpl);

}  // namespace autofill
