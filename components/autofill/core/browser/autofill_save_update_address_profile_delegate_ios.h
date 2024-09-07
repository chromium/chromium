// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
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
      std::optional<std::u16string> user_email,
      const std::string& locale,
      bool is_migration_to_account,
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

  // Returns the profile description shown in the migration prompt.
  std::u16string GetProfileDescriptionForMigrationPrompt() const;

  // Returns subtitle for the update modal.
  std::u16string GetSubtitle();

  // Returns the message button text.
  std::u16string GetMessageActionText() const;

  // Returns the data stored in the |profile_| corresponding to |type|.
  std::u16string GetProfileInfo(FieldType type) const;

  // Returns the profile difference map between |profile_| and
  // |original_profile_|.
  std::vector<ProfileValueDifference> GetProfileDiff() const;

  virtual void EditAccepted();
  void EditDeclined();
  void MessageTimeout();
  void MessageDeclined();
  void AutoDecline();
  virtual bool Never();

  void SetProfile(AutofillProfile* profile);

  const AutofillProfile* GetProfile() const;
  const AutofillProfile* GetOriginalProfile() const;

  // Getter and Setter for `is_infobar_visible_`.
  bool is_infobar_visible() const { return is_infobar_visible_; }
  void set_is_infobar_visible(bool is_infobar_visible) {
    is_infobar_visible_ = is_infobar_visible;
  }

  // ConfirmInfoBarDelegate
  int GetIconId() const override;
  std::u16string GetMessageText() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool Accept() override;
  bool Cancel() override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;

  bool IsMigrationToAccount() const { return is_migration_to_account_; }

  std::optional<std::u16string> UserAccountEmail() const { return user_email_; }

  // Returns true if the profile's record type is
  // `AutofillProfile::RecordType::kAccount`.
  bool IsProfileAnAccountProfile() const { return profile_.IsAccountProfile(); }

#if defined(UNIT_TEST)
  // Getter for |user_decision_|. Used for the testing purposes.
  AutofillClient::AddressPromptUserDecision user_decision() const {
    return user_decision_;
  }
#endif

 private:
  // Fires the |address_profile_save_prompt_callback_| callback with
  // |user_decision_|.
  void RunSaveAddressProfilePromptCallback();

  // Sets |user_decision_| based on |user_decision|.
  void SetUserDecision(AutofillClient::AddressPromptUserDecision user_decision);

  // The application locale.
  std::string locale_;

  // The profile that will be saved if the user accepts.
  AutofillProfile profile_;

  // The original profile that will be updated if the user accepts the update
  // prompt. NULL if saving a new profile.
  std::optional<AutofillProfile> original_profile_;

  // The callback to run once the user makes a decision.
  AutofillClient::AddressProfileSavePromptCallback
      address_profile_save_prompt_callback_;

  // True if the either of banner or modal is visible currently.
  bool is_infobar_visible_ = false;

  // Denotes if an account migration prompt should be shown.
  bool is_migration_to_account_;

  // Denotes the email address of the syncing account.
  std::optional<std::u16string> user_email_;

  // Records the last user decision based on the interactions with the
  // banner/modal to be sent with |address_profile_save_prompt_callback_|.
  AutofillClient::AddressPromptUserDecision user_decision_ =
      AutofillClient::AddressPromptUserDecision::kIgnored;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_DELEGATE_IOS_H_
