// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_profiles/save_address_profile_bubble_controller.h"

#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

SaveAddressProfileBubbleController::SaveAddressProfileBubbleController(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
}

SaveAddressProfileBubbleController::~SaveAddressProfileBubbleController() =
    default;

base::string16 SaveAddressProfileBubbleController::GetWindowTitle() const {
  return base::string16();
}

void SaveAddressProfileBubbleController::OfferSave(
    const AutofillProfile& profile,
    AutofillClient::AddressProfileSavePromptCallback
        address_profile_save_prompt_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view())
    return;
  address_profile_ = profile;
  address_profile_save_prompt_callback_ =
      std::move(address_profile_save_prompt_callback);
  Show();
}

void SaveAddressProfileBubbleController::OnBubbleClosed() {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
}

bool SaveAddressProfileBubbleController::HandleDidFinishRelevantNavigation() {
  return true;
}

PageActionIconType SaveAddressProfileBubbleController::GetPageActionIconType() {
  return PageActionIconType::kSaveCard;
}

void SaveAddressProfileBubbleController::DoShowBubble() {
  DCHECK(!bubble_view());
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  set_bubble_view(
      browser->window()
          ->GetAutofillBubbleHandler()
          ->ShowSaveAddressProfileBubble(web_contents(), this,
                                         /*is_user_gesture=*/false));
  DCHECK(bubble_view());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveAddressProfileBubbleController)

}  // namespace autofill
