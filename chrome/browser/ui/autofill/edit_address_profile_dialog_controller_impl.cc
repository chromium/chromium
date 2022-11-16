// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"

#include "base/types/optional_util.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

EditAddressProfileDialogControllerImpl::EditAddressProfileDialogControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<EditAddressProfileDialogControllerImpl>(
          *web_contents) {}

EditAddressProfileDialogControllerImpl::
    ~EditAddressProfileDialogControllerImpl() {
  HideDialog();
}

void EditAddressProfileDialogControllerImpl::OfferEdit(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  // Don't show the bubble if it's already visible, and inform the backend.
  if (dialog_view_) {
    std::move(address_profile_save_prompt_callback)
        .Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined,
             profile);
    return;
  }
  address_profile_to_edit_ = profile;
  original_profile_ = base::OptionalFromPtr(original_profile);
  address_profile_save_prompt_callback_ =
      std::move(address_profile_save_prompt_callback);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  dialog_view_ = browser->window()
                     ->GetAutofillBubbleHandler()
                     ->ShowEditAddressProfileDialog(web_contents(), this);
}

std::u16string EditAddressProfileDialogControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_TITLE);
}

std::u16string EditAddressProfileDialogControllerImpl::GetOkButtonLabel()
    const {
  return l10n_util::GetStringUTF16(
      original_profile_.has_value()
          ? IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_UPDATE
          : IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE);
}

const AutofillProfile&
EditAddressProfileDialogControllerImpl::GetProfileToEdit() const {
  return address_profile_to_edit_;
}

void EditAddressProfileDialogControllerImpl::OnUserDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision,
    const AutofillProfile& profile_with_edits) {
  // If the user accepted the flow, save the changes directly.
  if (decision ==
      AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted) {
    std::move(address_profile_save_prompt_callback_)
        .Run(decision, profile_with_edits);
    return;
  }
  // If the user hits "Cancel", reopen the previous prompt.
  SaveUpdateAddressProfileBubbleControllerImpl::CreateForWebContents(
      web_contents());
  SaveUpdateAddressProfileBubbleControllerImpl* controller =
      SaveUpdateAddressProfileBubbleControllerImpl::FromWebContents(
          web_contents());
  controller->OfferSave(
      address_profile_to_edit_, base::OptionalToPtr(original_profile_),
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      std::move(address_profile_save_prompt_callback_));
}

void EditAddressProfileDialogControllerImpl::OnDialogClosed() {
  if (address_profile_save_prompt_callback_) {
    OnUserDecision(
        AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
        address_profile_to_edit_);
  }
  dialog_view_ = nullptr;
}

void EditAddressProfileDialogControllerImpl::WebContentsDestroyed() {
  HideDialog();
}

void EditAddressProfileDialogControllerImpl::HideDialog() {
  if (dialog_view_) {
    dialog_view_->Hide();
    dialog_view_ = nullptr;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(EditAddressProfileDialogControllerImpl);

}  // namespace autofill
