// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace autofill {

// A delegate for the prompt that enables the user to allow or deny storing
// an address profile gathered from a form submission. Only used on iOS.
class AutofillSaveUpdateAddressProfileDelegateIOS
    : public ConfirmInfoBarDelegate {
 public:
  AutofillSaveUpdateAddressProfileDelegateIOS(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      const std::string& locale,
      AutofillClient::AddressProfileSavePromptCallback callback);
  AutofillSaveUpdateAddressProfileDelegateIOS(
      const AutofillSaveUpdateAddressProfileDelegateIOS&) = delete;
  AutofillSaveUpdateAddressProfileDelegateIOS& operator=(
      const AutofillSaveUpdateAddressProfileDelegateIOS&) = delete;
  ~AutofillSaveUpdateAddressProfileDelegateIOS() override;

  // Returns |delegate| as an AutofillSaveUpdateAddressProfileDelegateIOS, or
  // nullptr if it is of another type.
  static AutofillSaveUpdateAddressProfileDelegateIOS* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  // Returns the address in envelope style in the |profile_|.
  std::u16string GetEnvelopeStyleAddress() const;

  // Returns the phone number in the |profile_|.
  std::u16string GetPhoneNumber() const;

  // Returns the email address in the |profile_|.
  std::u16string GetEmailAddress() const;

  // Returns the subtitle text to be displayed in the save/update banner.
  std::u16string GetDescription() const;

  // Returns the message button text.
  std::u16string GetMessageActionText() const;

  // Uses |AutofillProfileComparator::GetSettingsVisibleProfileDifferenceMap| to
  // get profile difference map between |profile_| and |original_profile_|;
  base::flat_map<ServerFieldType, std::pair<std::u16string, std::u16string>>
  GetProfileDiff() const;

  const autofill::AutofillProfile* GetProfile() const;
  const autofill::AutofillProfile* GetOriginalProfile() const;
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

  // The application locale.
  std::string locale_;

  // The profile that will be saved if the user accepts.
  AutofillProfile profile_;

  // The original profile that will be updated if the user accepts the update
  // prompt. NULL if saving a new profile.
  base::Optional<AutofillProfile> original_profile_;

  // The callback to run once the user makes a decision.
  AutofillClient::AddressProfileSavePromptCallback
      address_profile_save_prompt_callback_;

  // True if the AddressProfile modal dialog is shown.
  bool modal_is_shown_ = false;

  // True if the modal dialog was presented and then dismissed by the user.
  bool modal_is_dismissed_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_
