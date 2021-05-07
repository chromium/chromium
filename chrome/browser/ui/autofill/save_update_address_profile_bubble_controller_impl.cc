// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

SaveUpdateAddressProfileBubbleControllerImpl::
    SaveUpdateAddressProfileBubbleControllerImpl(
        content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
}

SaveUpdateAddressProfileBubbleControllerImpl::
    ~SaveUpdateAddressProfileBubbleControllerImpl() = default;

void SaveUpdateAddressProfileBubbleControllerImpl::OfferSave(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    AutofillClient::SaveAddressProfilePromptOptions options,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;
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
  // TODO(crbug.com/1167060): Use internationalized string upon having final
  // strings.
  // TODO(crbug.com/1167060): Update prompt title should reflect the fields that
  // are being updated.
  return original_profile_ ? u"Update Address?" : u"Save Address?";
}

const AutofillProfile&
SaveUpdateAddressProfileBubbleControllerImpl::GetProfileToSave() const {
  return address_profile_;
}

const AutofillProfile*
SaveUpdateAddressProfileBubbleControllerImpl::GetOriginalProfile() const {
  return base::OptionalOrNullptr(original_profile_);
}

void SaveUpdateAddressProfileBubbleControllerImpl::OnUserDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  set_bubble_view(nullptr);

  std::move(address_profile_save_prompt_callback_)
      .Run(decision, address_profile_);
}

void SaveUpdateAddressProfileBubbleControllerImpl::OnEditButtonClicked() {
  HideBubble();
  EditAddressProfileDialogControllerImpl::CreateForWebContents(web_contents());
  EditAddressProfileDialogControllerImpl* controller =
      EditAddressProfileDialogControllerImpl::FromWebContents(web_contents());
  controller->OfferEdit(address_profile_, GetOriginalProfile(),
                        std::move(address_profile_save_prompt_callback_));
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

AutofillBubbleBase*
SaveUpdateAddressProfileBubbleControllerImpl::GetSaveBubbleView() const {
  return bubble_view();
}

PageActionIconType
SaveUpdateAddressProfileBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kSaveAutofillAddress;
}

void SaveUpdateAddressProfileBubbleControllerImpl::DoShowBubble() {
  DCHECK(!bubble_view());
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!original_profile_) {
    // This must is a save prompt.
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

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveUpdateAddressProfileBubbleControllerImpl)

}  // namespace autofill
