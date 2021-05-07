// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"

#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/browser/autofill_client.h"
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
    const AutofillProfile* original_profile,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  address_profile_to_edit_ = profile;
  original_profile_ = base::OptionalFromPtr(original_profile);
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
  return original_profile_ ? u"Update Address?" : u"Save Address?";
}

const AutofillProfile&
EditAddressProfileDialogControllerImpl::GetProfileToEdit() const {
  return address_profile_to_edit_;
}

void EditAddressProfileDialogControllerImpl::OnUserDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision,
    const AutofillProfile& profile_with_edits) {
  edit_dialog_ = nullptr;
  // Pass back the address profile with or without edits depending on user
  // decision.
  const AutofillProfile& profile =
      decision == AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted
          ? profile_with_edits
          : address_profile_to_edit_;

  SaveUpdateAddressProfileBubbleControllerImpl::CreateForWebContents(
      web_contents());
  SaveUpdateAddressProfileBubbleControllerImpl* controller =
      SaveUpdateAddressProfileBubbleControllerImpl::FromWebContents(
          web_contents());
  controller->OfferSave(
      profile, base::OptionalOrNullptr(original_profile_),
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      std::move(address_profile_save_prompt_callback_));
}

void EditAddressProfileDialogControllerImpl::OnDialogClosed() {
  edit_dialog_ = nullptr;
  if (address_profile_save_prompt_callback_) {
    OnUserDecision(
        AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
        address_profile_to_edit_);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(EditAddressProfileDialogControllerImpl)

}  // namespace autofill
