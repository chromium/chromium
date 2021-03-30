// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"

#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

EditAddressProfileDialogControllerImpl::EditAddressProfileDialogControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
}

EditAddressProfileDialogControllerImpl::
    ~EditAddressProfileDialogControllerImpl() = default;

void EditAddressProfileDialogControllerImpl::OfferEdit(
    const AutofillProfile& profile,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  address_profile_to_edit_ = profile;
  address_profile_save_prompt_callback_ =
      std::move(address_profile_save_prompt_callback);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  edit_dialog_ = browser->window()
                     ->GetAutofillBubbleHandler()
                     ->ShowEditAddressProfileDialog(web_contents(), this);
}

std::u16string EditAddressProfileDialogControllerImpl::GetWindowTitle() const {
  // TODO(crbug.com/1167060): Use internationalized string upon having final
  // strings.
  return u"Save Address?";
}

const AutofillProfile&
EditAddressProfileDialogControllerImpl::GetProfileToEdit() const {
  return address_profile_to_edit_;
}

void EditAddressProfileDialogControllerImpl::OnUserDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision,
    const AutofillProfile& profile_with_edits) {
  edit_dialog_ = nullptr;
  std::move(address_profile_save_prompt_callback_)
      .Run(decision, profile_with_edits);
}

void EditAddressProfileDialogControllerImpl::OnDialogClosed() {
  edit_dialog_ = nullptr;
  if (address_profile_save_prompt_callback_) {
    std::move(address_profile_save_prompt_callback_)
        .Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
             address_profile_to_edit_);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(EditAddressProfileDialogControllerImpl)

}  // namespace autofill
