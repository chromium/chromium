// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveUpdateAddressProfileBubbleControllerImpl::
    SaveUpdateAddressProfileBubbleControllerImpl(
        content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<
          SaveUpdateAddressProfileBubbleControllerImpl>(*web_contents) {}

SaveUpdateAddressProfileBubbleControllerImpl::
    ~SaveUpdateAddressProfileBubbleControllerImpl() {
  // `address_profile_save_prompt_callback_` must have been invoked before
  // destroying the controller to inform the backend of the output of the
  // save/update flow. It's either invoked upon user action when accepting
  // or rejecting the flow, or in cases when users ignore it, it's invoked
  // when the web contents are destroyed.
  DCHECK(address_profile_save_prompt_callback_.is_null());
}

void SaveUpdateAddressProfileBubbleControllerImpl::OfferSave(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::SaveAddressProfilePromptOptions options,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  // Don't show the bubble if it's already visible, and inform the backend.
  if (bubble_view()) {
    std::move(address_profile_save_prompt_callback)
        .Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined,
             profile);
    return;
  }
  // If the user closed the bubble of the previous import process using the
  // "Close" button without making a decision to "Accept" or "Deny" the prompt,
  // a fallback icon is shown, so the user can get back to the prompt. In this
  // specific scenario the import process is considered in progress (since the
  // backend didn't hear back via the callback yet), but hidden. When a second
  // prompt arrives, we finish the previous import process as "Ignored", before
  // showing the 2nd prompt.
  if (address_profile_save_prompt_callback_) {
    std::move(address_profile_save_prompt_callback_)
        .Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
             address_profile_);
  }

  address_profile_ = profile;
  original_profile_ = base::OptionalFromPtr(original_profile);
  address_profile_save_prompt_callback_ =
      std::move(address_profile_save_prompt_callback);
  shown_by_user_gesture_ = false;
  if (options.show_prompt)
    Show();
}

std::u16string SaveUpdateAddressProfileBubbleControllerImpl::GetWindowTitle()
    const {
  return l10n_util::GetStringUTF16(
      IsSaveBubble() ? IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE
                     : IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
}

const AutofillProfile&
SaveUpdateAddressProfileBubbleControllerImpl::GetProfileToSave() const {
  return address_profile_;
}

const AutofillProfile*
SaveUpdateAddressProfileBubbleControllerImpl::GetOriginalProfile() const {
  return base::OptionalToPtr(original_profile_);
}

void SaveUpdateAddressProfileBubbleControllerImpl::OnUserDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  if (address_profile_save_prompt_callback_) {
    std::move(address_profile_save_prompt_callback_)
        .Run(decision, address_profile_);
  }
}

void SaveUpdateAddressProfileBubbleControllerImpl::OnEditButtonClicked() {
  EditAddressProfileDialogControllerImpl::CreateForWebContents(web_contents());
  EditAddressProfileDialogControllerImpl* controller =
      EditAddressProfileDialogControllerImpl::FromWebContents(web_contents());
  controller->OfferEdit(address_profile_, GetOriginalProfile(),
                        std::move(address_profile_save_prompt_callback_));
  HideBubble();
}

void SaveUpdateAddressProfileBubbleControllerImpl::OnBubbleClosed() {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
}

void SaveUpdateAddressProfileBubbleControllerImpl::OnPageActionIconClicked() {
  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;
  shown_by_user_gesture_ = true;
  Show();
}

bool SaveUpdateAddressProfileBubbleControllerImpl::IsBubbleActive() const {
  return !address_profile_save_prompt_callback_.is_null();
}

std::u16string
SaveUpdateAddressProfileBubbleControllerImpl::GetPageActionIconTootip() const {
  return GetWindowTitle();
}

AutofillBubbleBase*
SaveUpdateAddressProfileBubbleControllerImpl::GetBubbleView() const {
  return bubble_view();
}

bool SaveUpdateAddressProfileBubbleControllerImpl::IsSaveBubble() const {
  return !original_profile_;
}

void SaveUpdateAddressProfileBubbleControllerImpl::WebContentsDestroyed() {
  AutofillBubbleControllerBase::WebContentsDestroyed();

  OnUserDecision(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored);
}

PageActionIconType
SaveUpdateAddressProfileBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kSaveAutofillAddress;
}

void SaveUpdateAddressProfileBubbleControllerImpl::DoShowBubble() {
  DCHECK(!bubble_view());
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (IsSaveBubble()) {
    set_bubble_view(browser->window()
                        ->GetAutofillBubbleHandler()
                        ->ShowSaveAddressProfileBubble(web_contents(), this,
                                                       shown_by_user_gesture_));
  } else {
    // This is an update prompt.
    set_bubble_view(browser->window()
                        ->GetAutofillBubbleHandler()
                        ->ShowUpdateAddressProfileBubble(
                            web_contents(), this, shown_by_user_gesture_));
  }
  DCHECK(bubble_view());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveUpdateAddressProfileBubbleControllerImpl);

}  // namespace autofill
