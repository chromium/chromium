// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_ADDRESS_PROFILE_DELEGATE_IOS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_ADDRESS_PROFILE_DELEGATE_IOS_H_

#include <memory>

#include "base/callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace autofill {

// A delegate for the prompt that enables the user to allow or deny storing
// an address profile gathered from a form submission. Only used on iOS.
class AutofillSaveAddressProfileDelegateIOS : public ConfirmInfoBarDelegate {
 public:
  AutofillSaveAddressProfileDelegateIOS(
      const AutofillProfile& profile,
      AutofillClient::AddressProfileSavePromptCallback callback);
  AutofillSaveAddressProfileDelegateIOS(
      const AutofillSaveAddressProfileDelegateIOS&) = delete;
  AutofillSaveAddressProfileDelegateIOS& operator=(
      const AutofillSaveAddressProfileDelegateIOS&) = delete;
  ~AutofillSaveAddressProfileDelegateIOS() override;

  // Returns |delegate| as an AutofillSaveAddressProfileDelegateIOS, or nullptr
  // if it is of another type.
  static AutofillSaveAddressProfileDelegateIOS* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  std::u16string GetMessageDescriptionText() const;
  std::u16string GetMessageActionText() const;
  const autofill::AutofillProfile* GetProfile() const;
  void set_modal_is_shown_to_true() { modal_is_shown_ = true; }

  void set_modal_is_dismissed_to_true() { modal_is_dismissed_ = true; }

  // ConfirmInfoBarDelegate
  int GetIconId() const override;
  std::u16string GetMessageText() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

 private:
  // Fires the |address_profile_save_prompt_callback_| callback.
  void RunSaveAddressProfilePromptCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision decision);

  // The profile that will be saved if the user accepts.
  AutofillProfile profile_;

  // The callback to run once the user makes a decision.
  AutofillClient::AddressProfileSavePromptCallback
      address_profile_save_prompt_callback_;

  // True if the AddressProfile modal dialog is shown.
  bool modal_is_shown_ = false;

  // True if the modal dialog was presented and then dismissed by the user.
  bool modal_is_dismissed_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_ADDRESS_PROFILE_DELEGATE_IOS_H_
