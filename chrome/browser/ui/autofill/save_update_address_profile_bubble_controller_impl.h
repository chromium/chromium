// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_IMPL_H_

#include <string>

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_icon_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class AutofillBubbleBase;

// The controller functionality for SaveAddressProfileView.
class SaveUpdateAddressProfileBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public SaveUpdateAddressProfileBubbleController,
      public SaveUpdateAddressProfileIconController,
      public content::WebContentsUserData<
          SaveUpdateAddressProfileBubbleControllerImpl> {
 public:
  SaveUpdateAddressProfileBubbleControllerImpl(
      const SaveUpdateAddressProfileBubbleControllerImpl&) = delete;
  SaveUpdateAddressProfileBubbleControllerImpl& operator=(
      const SaveUpdateAddressProfileBubbleControllerImpl&) = delete;
  ~SaveUpdateAddressProfileBubbleControllerImpl() override;

  // Sets up the controller and offers to save the `profile`. If
  // `original_profile` is not nullptr, it will be updated of the user accepts
  // the offer. `address_profile_save_prompt_callback` will be invoked once the
  // user makes a decision with respect to the offer-to-save prompt.
  // `options` carries extra configuration for opening the prompt.
  void OfferSave(const AutofillProfile& profile,
                 const AutofillProfile* original_profile,
                 AutofillClient::SaveAddressProfilePromptOptions options,
                 AutofillClient::AddressProfileSavePromptCallback
                     address_profile_save_prompt_callback);

  // SaveUpdateAddressProfileBubbleController:
  std::u16string GetWindowTitle() const override;
  absl::optional<HeaderImages> GetHeaderImages() const override;
  std::u16string GetBodyText() const override;
  std::u16string GetAddressSummary() const override;
  std::u16string GetProfileEmail() const override;
  std::u16string GetProfilePhone() const override;
  std::u16string GetOkButtonLabel() const override;
  AutofillClient::SaveAddressProfileOfferUserDecision GetCancelCallbackValue()
      const override;
  std::u16string GetFooterMessage() const override;
  const AutofillProfile& GetProfileToSave() const override;
  const AutofillProfile* GetOriginalProfile() const override;
  void OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision decision,
      base::optional_ref<const AutofillProfile> profile) override;
  void OnEditButtonClicked() override;
  void OnBubbleClosed() override;

  // SaveAddressProfileIconController:
  void OnPageActionIconClicked() override;
  bool IsBubbleActive() const override;
  std::u16string GetPageActionIconTootip() const override;
  AutofillBubbleBase* GetBubbleView() const override;
  bool IsSaveBubble() const override;

 protected:
  // AutofillBubbleControllerBase:
  void WebContentsDestroyed() override;
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  explicit SaveUpdateAddressProfileBubbleControllerImpl(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      SaveUpdateAddressProfileBubbleControllerImpl>;

  base::WeakPtr<SaveUpdateAddressProfileBubbleController> GetWeakPtr();

  std::u16string GetEditorFooterMessage() const;

  // Callback to run once the user makes a decision with respect to the saving
  // the address profile.
  AutofillClient::AddressProfileSavePromptCallback
      address_profile_save_prompt_callback_;

  // Contains the details of the address profile that will be saved if the user
  // accepts.
  AutofillProfile address_profile_;

  // Contains the details of the address profile that will be updated if the
  // user accepts the prompt.
  absl::optional<AutofillProfile> original_profile_;

  // Whether the bubble is going to be shown upon user gesture (e.g. click on
  // the page action icon) or automatically (e.g. upon detection of an address
  // during form submission).
  bool shown_by_user_gesture_ = false;

  // Whether the bubble prompts to save (migrate) the profile into account.
  bool is_migration_to_account_ = false;

  std::string app_locale_;

  base::WeakPtrFactory<SaveUpdateAddressProfileBubbleController>
      weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_IMPL_H_
