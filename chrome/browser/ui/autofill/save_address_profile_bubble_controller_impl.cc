// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_address_profile_bubble_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

SaveAddressProfileBubbleControllerImpl::SaveAddressProfileBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
}

SaveAddressProfileBubbleControllerImpl::
    ~SaveAddressProfileBubbleControllerImpl() = default;

void SaveAddressProfileBubbleControllerImpl::OfferSave(
    const AutofillProfile& profile,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;
  address_profile_ = profile;
  address_profile_save_prompt_callback_ =
      std::move(address_profile_save_prompt_callback);
  shown_by_user_gesture_ = false;
  Show();
}

base::string16 SaveAddressProfileBubbleControllerImpl::GetWindowTitle() const {
  // TODO(crbug.com/1167060): Use ineternationalized string upon having final
  // strings.
  return base::UTF8ToUTF16("Save Address?");
}

const AutofillProfile&
SaveAddressProfileBubbleControllerImpl::GetProfileToSave() const {
  return address_profile_;
}

void SaveAddressProfileBubbleControllerImpl::OnUserDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  std::move(address_profile_save_prompt_callback_)
      .Run(decision, address_profile_);
}

void SaveAddressProfileBubbleControllerImpl::OnBubbleClosed() {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
}

void SaveAddressProfileBubbleControllerImpl::OnPageActionIconClicked() {
  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;
  shown_by_user_gesture_ = true;
  Show();
}

bool SaveAddressProfileBubbleControllerImpl::IsBubbleActive() const {
  return bubble_view() != nullptr;
}

AutofillBubbleBase* SaveAddressProfileBubbleControllerImpl::GetSaveBubbleView()
    const {
  return bubble_view();
}

PageActionIconType
SaveAddressProfileBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kSaveCard;
}

void SaveAddressProfileBubbleControllerImpl::DoShowBubble() {
  DCHECK(!bubble_view());
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  set_bubble_view(browser->window()
                      ->GetAutofillBubbleHandler()
                      ->ShowSaveAddressProfileBubble(web_contents(), this,
                                                     shown_by_user_gesture_));
  DCHECK(bubble_view());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveAddressProfileBubbleControllerImpl)

}  // namespace autofill
