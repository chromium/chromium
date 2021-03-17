// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_IMPL_H_

#include <string>

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/save_address_profile_bubble_controller.h"
#include "chrome/browser/ui/autofill/save_address_profile_icon_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class AutofillBubbleBase;

// The controller functionality for SaveAddressProfileView.
class SaveAddressProfileBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public SaveAddressProfileBubbleController,
      public SaveAddressProfileIconController,
      public content::WebContentsUserData<
          SaveAddressProfileBubbleControllerImpl> {
 public:
  SaveAddressProfileBubbleControllerImpl(
      const SaveAddressProfileBubbleControllerImpl&) = delete;
  SaveAddressProfileBubbleControllerImpl& operator=(
      const SaveAddressProfileBubbleControllerImpl&) = delete;
  ~SaveAddressProfileBubbleControllerImpl() override;

  // Sets up the controller and offers to save the |profile|.
  // |address_profile_save_prompt_callback| will be invoked once the user makes
  // a decision with respect to the offer-to-save prompt.
  void OfferSave(const AutofillProfile& profile,
                 AutofillClient::AddressProfileSavePromptCallback
                     address_profile_save_prompt_callback);

  // SaveAddressProfileBubbleController:
  std::u16string GetWindowTitle() const override;
  const AutofillProfile& GetProfileToSave() const override;
  void OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision decision) override;
  void OnEditButtonClicked() override;
  void OnBubbleClosed() override;

  // SaveAddressProfileIconController:
  void OnPageActionIconClicked() override;
  bool IsBubbleActive() const override;
  AutofillBubbleBase* GetSaveBubbleView() const override;

 protected:
  // AutofillBubbleControllerBase:
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  explicit SaveAddressProfileBubbleControllerImpl(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      SaveAddressProfileBubbleControllerImpl>;

  // Callback to run once the user makes a decision with respect to the saving
  // the address profile.
  AutofillClient::AddressProfileSavePromptCallback
      address_profile_save_prompt_callback_;

  // Contains the details of the address profile that will be saved if the user
  // accepts.
  AutofillProfile address_profile_;

  // Whether the bubble is going to be shown upon user gesture (e.g. click on
  // the page action icon) or automatically (e.g. upon detection of an address
  // during form submission).
  bool shown_by_user_gesture_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_IMPL_H_
