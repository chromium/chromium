// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ADDRESS_PROFILES_SAVE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ADDRESS_PROFILES_SAVE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class SaveAddressProfileView;

// The controller functionality for SaveAddressProfileView.
class SaveAddressProfileBubbleController
    : public AutofillBubbleControllerBase,
      public content::WebContentsUserData<SaveAddressProfileBubbleController> {
 public:
  SaveAddressProfileBubbleController(
      const SaveAddressProfileBubbleController&) = delete;
  SaveAddressProfileBubbleController& operator=(
      const SaveAddressProfileBubbleController&) = delete;
  ~SaveAddressProfileBubbleController() override;

  base::string16 GetWindowTitle() const;

  // Sets up the controller and offers to save the |profile|.
  // |address_profile_save_prompt_callback| will be invoked once the user makes
  // a decision with respect to the offer-to-save prompt.
  void OfferSave(const AutofillProfile& profile,
                 AutofillClient::AddressProfileSavePromptCallback
                     address_profile_save_prompt_callback);

  void OnBubbleClosed();

 protected:
  // AutofillBubbleControllerBase::
  bool HandleDidFinishRelevantNavigation() override;
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  explicit SaveAddressProfileBubbleController(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<SaveAddressProfileBubbleController>;

  // Callback to run once the user makes a decision with respect to the saving
  // the address profile.
  AutofillClient::AddressProfileSavePromptCallback
      address_profile_save_prompt_callback_;

  // Contains the details of the address profile that will be saved if the user
  // accepts.
  AutofillProfile address_profile_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_ADDRESS_PROFILES_SAVE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_
