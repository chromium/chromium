// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_IMPL_H_

#include <string>

#include "chrome/browser/ui/autofill/address_bubble_controller_delegate.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_icon_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class AutofillBubbleBase;

// The controller of the address page action icon, see the implementation of
// the `SaveUpdateAddressProfileIconController` interface. Different types of
// address bubbles can be bound to the icon (e.g. save or update address). This
// controller acts as the delegate for them (hence implementing
// `AddressBubbleControllerDelegate`) to support higher level flows like saving
// an address with editing.
// Only single instance of this controller exists for a `WebContents`, to use
// it for different flows it must be reconfigured, see the arguments of
// the `OfferSave()` method.
class SaveUpdateAddressProfileBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public SaveUpdateAddressProfileIconController,
      public content::WebContentsUserData<
          SaveUpdateAddressProfileBubbleControllerImpl>,
      public AddressBubbleControllerDelegate {
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

  // AddressBubbleControllerDelegate:
  void OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision decision,
      base::optional_ref<const AutofillProfile> profile) override;
  void OnEditButtonClicked(
      const std::u16string& editor_footer_message) override;
  void OnBubbleClosed() override;

  // SaveAddressProfileIconController:
  void OnPageActionIconClicked() override;
  bool IsBubbleActive() const override;
  std::u16string GetPageActionIconTootip() const override;
  AutofillBubbleBase* GetBubbleView() const override;

  base::WeakPtr<AddressBubbleControllerDelegate> GetWeakPtr();

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

  bool IsSaveBubble() const;

  // Callback to run once the user makes a decision with respect to the saving
  // the address profile.
  AutofillClient::AddressProfileSavePromptCallback
      address_profile_save_prompt_callback_;

  // Contains the details of the address profile that will be saved if the user
  // accepts.
  std::optional<AutofillProfile> address_profile_;

  // Contains the details of the address profile that will be updated if the
  // user accepts the prompt.
  std::optional<AutofillProfile> original_profile_;

  // Whether the bubble is going to be shown upon user gesture (e.g. click on
  // the page action icon) or automatically (e.g. upon detection of an address
  // during form submission).
  bool shown_by_user_gesture_ = false;

  // Whether the bubble prompts to save (migrate) the profile into account.
  bool is_migration_to_account_ = false;

  std::string app_locale_;

  base::WeakPtrFactory<SaveUpdateAddressProfileBubbleControllerImpl>
      weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_IMPL_H_
