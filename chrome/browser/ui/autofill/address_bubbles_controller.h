// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ADDRESS_BUBBLES_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ADDRESS_BUBBLES_CONTROLLER_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ui/autofill/address_bubble_controller_delegate.h"
#include "chrome/browser/ui/autofill/address_bubbles_icon_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class AutofillBubbleBase;

// The controller of the address page action icon, see the implementation of
// the `AddressBubblesIconController` interface. Different types of address
// bubbles can be bound to the icon (e.g. save or update address). This
// controller acts as the delegate for them (hence implementing
// `AddressBubbleControllerDelegate`) to support higher level flows like saving
// an address with editing.
// Only single instance of this controller exists for a `WebContents`, to use
// it for different flows it must be reconfigured, see the arguments of
// the `OfferSave()` method.
class AddressBubblesController
    : public AutofillBubbleControllerBase,
      public AddressBubblesIconController,
      public content::WebContentsUserData<
          AddressBubblesController>,
      public AddressBubbleControllerDelegate {
 public:
  AddressBubblesController(
      const AddressBubblesController&) = delete;
  AddressBubblesController& operator=(
      const AddressBubblesController&) = delete;
  ~AddressBubblesController() override;

  // Sets up the controller and offers to save or the `profile`. If
  // `original_profile` is not nullptr, it will be updated if the user accepts
  // the offer. `callback` will be invoked once the user makes a decision with
  // respect to the prompt. `is_migration_to_account` is relevant for the save
  // case only and makes the bubble open in a special mode for saving `profile`
  // in user's Google account.
  static void SetUpAndShowSaveOrUpdateAddressBubble(
      content::WebContents* web_contents,
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      bool is_migration_to_account,
      AutofillClient::AddressProfileSavePromptCallback callback);

  static void SetUpAndShowAddNewAddressBubble(
      content::WebContents* web_contents,
      AutofillClient::AddressProfileSavePromptCallback callback);

  // AddressBubbleControllerDelegate:
  void ShowEditor(const AutofillProfile& address_profile,
                  const std::u16string& title_override,
                  const std::u16string& editor_footer_message,
                  bool is_editing_existing_address) override;
  void OnUserDecision(
      AutofillClient::AddressPromptUserDecision decision,
      base::optional_ref<const AutofillProfile> profile) override;
  void OnBubbleClosed() override;

  // SaveAddressProfileIconController:
  void OnIconClicked() override;
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
  using ShowBubbleViewCallback = base::RepeatingCallback<AutofillBubbleBase*(
      content::WebContents*,
      /*shown_by_user_gesture=*/bool,
      base::WeakPtr<AddressBubbleControllerDelegate>)>;

  explicit AddressBubblesController(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      AddressBubblesController>;

  // TODO(crbug.com/325440757): Remove `profile` and `original_profile`, put
  // them in specific bubble controllers.
  void SetUpAndShowBubble(ShowBubbleViewCallback show_bubble_view_callback,
                          std::u16string page_action_icon_tootip,
                          bool is_migration_to_account,
                          AutofillClient::AddressProfileSavePromptCallback
                              address_profile_save_prompt_callback);

  // Maybe shows the iOS bubble promo after the user accepts to save their
  // address information.
  void MaybeShowIOSDektopAddressPromo();

  // Callback to run once the user makes a decision with respect to the saving
  // the address profile.
  AutofillClient::AddressProfileSavePromptCallback
      address_profile_save_prompt_callback_;

  // Whether the bubble is going to be shown upon user gesture (e.g. click on
  // the page action icon) or automatically (e.g. upon detection of an address
  // during form submission).
  bool shown_by_user_gesture_ = false;

  // Whether the bubble prompts to save (migrate) the profile into account.
  bool is_migration_to_account_ = false;

  // The callback to create and show the bubble. It defines the appearance of
  // the bubble and contains some specific logic. The controller doesn't take
  // the ownership of the instance returned (it only hides the bubble),
  // the hosting widget is expected to be the owner.
  ShowBubbleViewCallback show_bubble_view_callback_;

  std::u16string page_action_icon_tootip_;

  std::string app_locale_;

  base::WeakPtrFactory<AddressBubblesController>
      weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_ADDRESS_BUBBLES_CONTROLLER_H_
