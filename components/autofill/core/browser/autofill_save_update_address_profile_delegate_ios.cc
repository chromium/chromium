// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/ios/common/features.h"
#include "components/grit/components_scaled_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillSaveUpdateAddressProfileDelegateIOS::
    AutofillSaveUpdateAddressProfileDelegateIOS(
        const AutofillProfile& profile,
        const AutofillProfile* original_profile,
        std::optional<std::u16string> user_email,
        const std::string& locale,
        bool is_migration_to_account,
        AutofillClient::AddressProfileSavePromptCallback callback)
    : locale_(locale),
      profile_(profile),
      original_profile_(base::OptionalFromPtr(original_profile)),
      address_profile_save_prompt_callback_(std::move(callback)),
      is_migration_to_account_(is_migration_to_account),
      user_email_(user_email) {}

AutofillSaveUpdateAddressProfileDelegateIOS::
    ~AutofillSaveUpdateAddressProfileDelegateIOS() {
  // If the user has navigated away without saving the modal, then the
  // |address_profile_save_prompt_callback_| is run here.
  if (!address_profile_save_prompt_callback_.is_null()) {
    DCHECK(user_decision_ !=
               AutofillClient::AddressPromptUserDecision::kAccepted &&
           user_decision_ !=
               AutofillClient::AddressPromptUserDecision::kEditAccepted &&
           user_decision_ != AutofillClient::AddressPromptUserDecision::kNever);
    RunSaveAddressProfilePromptCallback();
  }
}

// static
AutofillSaveUpdateAddressProfileDelegateIOS*
AutofillSaveUpdateAddressProfileDelegateIOS::FromInfobarDelegate(
    infobars::InfoBarDelegate* delegate) {
  return delegate->GetIdentifier() ==
                 AUTOFILL_ADDRESS_PROFILE_INFOBAR_DELEGATE_IOS
             ? static_cast<AutofillSaveUpdateAddressProfileDelegateIOS*>(
                   delegate)
             : nullptr;
}

std::u16string
AutofillSaveUpdateAddressProfileDelegateIOS::GetEnvelopeStyleAddress() const {
  return ::autofill::GetEnvelopeStyleAddress(profile_, locale_,
                                             /*include_recipient=*/true,
                                             /*include_country=*/true);
}

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::GetPhoneNumber()
    const {
  return GetProfileInfo(PHONE_HOME_WHOLE_NUMBER);
}

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::GetEmailAddress()
    const {
  return GetProfileInfo(EMAIL_ADDRESS);
}

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::
    GetProfileDescriptionForMigrationPrompt() const {
  return ::autofill::GetProfileSummaryForMigrationPrompt(profile_, locale_);
}

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::GetDescription()
    const {
  if (is_migration_to_account_) {
    return l10n_util::GetStringUTF16(
        IDS_IOS_AUTOFILL_MIGRATE_ADDRESS_IN_ACCOUNT_MESSAGE_SUBTITLE);
  }
  if (IsProfileAnAccountProfile() && !original_profile_.has_value()) {
    DCHECK(user_email_);
    return l10n_util::GetStringFUTF16(
        IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_MESSAGE_SUBTITLE,
        *user_email_);
  }
  return GetProfileDescription(
      original_profile_ ? *original_profile_ : profile_, locale_,
      /*include_address_and_contacts=*/true);
}

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::GetSubtitle() {
  DCHECK(original_profile_);
  std::vector<ProfileValueDifference> differences =
      GetProfileDifferenceForUi(original_profile_.value(), profile_, locale_);
  bool address_updated = base::Contains(differences, ADDRESS_HOME_ADDRESS,
                                        &ProfileValueDifference::type);
  return GetProfileDescription(
      original_profile_.value(), locale_,
      /*include_address_and_contacts=*/!address_updated);
}

std::u16string
AutofillSaveUpdateAddressProfileDelegateIOS::GetMessageActionText() const {
  return l10n_util::GetStringUTF16(
      original_profile_ ? IDS_IOS_AUTOFILL_UPDATE_ADDRESS_MESSAGE_PRIMARY_ACTION
                        : IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_PRIMARY_ACTION);
}

const AutofillProfile* AutofillSaveUpdateAddressProfileDelegateIOS::GetProfile()
    const {
  return &profile_;
}

const AutofillProfile*
AutofillSaveUpdateAddressProfileDelegateIOS::GetOriginalProfile() const {
  return base::OptionalToPtr(original_profile_);
}

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::GetProfileInfo(
    FieldType type) const {
  return profile_.GetInfo(type, locale_);
}

std::vector<ProfileValueDifference>
AutofillSaveUpdateAddressProfileDelegateIOS::GetProfileDiff() const {
  return GetProfileDifferenceForUi(*GetProfile(), *GetOriginalProfile(),
                                   locale_);
}

void AutofillSaveUpdateAddressProfileDelegateIOS::EditAccepted() {
  if (address_profile_save_prompt_callback_.is_null()) {
    // From the crash logs in crbug.com/1408890, it appears that there are
    // multiple calls to this method when the edit button is pressed so return
    // early if the callback has already been executed.
    return;
  }

  user_decision_ = AutofillClient::AddressPromptUserDecision::kEditAccepted;
  RunSaveAddressProfilePromptCallback();
}

void AutofillSaveUpdateAddressProfileDelegateIOS::EditDeclined() {
  SetUserDecision(AutofillClient::AddressPromptUserDecision::kEditDeclined);
}

void AutofillSaveUpdateAddressProfileDelegateIOS::MessageTimeout() {
  SetUserDecision(AutofillClient::AddressPromptUserDecision::kMessageTimeout);
}

void AutofillSaveUpdateAddressProfileDelegateIOS::MessageDeclined() {
  SetUserDecision(AutofillClient::AddressPromptUserDecision::kMessageDeclined);
}

void AutofillSaveUpdateAddressProfileDelegateIOS::AutoDecline() {
  SetUserDecision(AutofillClient::AddressPromptUserDecision::kAutoDeclined);
}

bool AutofillSaveUpdateAddressProfileDelegateIOS::Never() {
  SetUserDecision(AutofillClient::AddressPromptUserDecision::kNever);
  RunSaveAddressProfilePromptCallback();
  return true;
}

void AutofillSaveUpdateAddressProfileDelegateIOS::SetProfile(
    AutofillProfile* profile) {
  profile_ = *profile;
}

bool AutofillSaveUpdateAddressProfileDelegateIOS::Accept() {
  user_decision_ = AutofillClient::AddressPromptUserDecision::kAccepted;
  RunSaveAddressProfilePromptCallback();
  return true;
}

bool AutofillSaveUpdateAddressProfileDelegateIOS::Cancel() {
  SetUserDecision(AutofillClient::AddressPromptUserDecision::kDeclined);
  return true;
}

bool AutofillSaveUpdateAddressProfileDelegateIOS::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate->GetIdentifier() == GetIdentifier();
}

int AutofillSaveUpdateAddressProfileDelegateIOS::GetIconId() const {
  NOTREACHED_IN_MIGRATION();
  return IDR_INFOBAR_AUTOFILL_CC;
}

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::GetMessageText()
    const {
  if (is_migration_to_account_) {
    return l10n_util::GetStringUTF16(
        IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_MESSAGE_TITLE);
  }
  return l10n_util::GetStringUTF16(
      original_profile_ ? IDS_IOS_AUTOFILL_UPDATE_ADDRESS_MESSAGE_TITLE
                        : IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_TITLE);
}

infobars::InfoBarDelegate::InfoBarIdentifier
AutofillSaveUpdateAddressProfileDelegateIOS::GetIdentifier() const {
  return AUTOFILL_ADDRESS_PROFILE_INFOBAR_DELEGATE_IOS;
}

bool AutofillSaveUpdateAddressProfileDelegateIOS::ShouldExpire(
    const NavigationDetails& details) const {
  const bool from_user_gesture =
      !base::FeatureList::IsEnabled(kAutofillStickyInfobarIos) ||
      details.has_user_gesture;

  // Expire the Infobar unless the navigation was triggered by the form that
  // presented the Infobar, or the navigation is a redirect.
  // Also, expire the infobar if the navigation is to a different page.
  return !details.is_form_submission && !details.is_redirect &&
         from_user_gesture && ConfirmInfoBarDelegate::ShouldExpire(details);
}

void AutofillSaveUpdateAddressProfileDelegateIOS::
    RunSaveAddressProfilePromptCallback() {
  std::move(address_profile_save_prompt_callback_)
      .Run(user_decision_,
           user_decision_ ==
                   AutofillClient::AddressPromptUserDecision::kEditAccepted
               ? base::optional_ref(profile_)
               : std::nullopt);
}

void AutofillSaveUpdateAddressProfileDelegateIOS::SetUserDecision(
    AutofillClient::AddressPromptUserDecision user_decision) {
  if (user_decision ==
          AutofillClient::AddressPromptUserDecision::kMessageTimeout &&
      user_decision_ ==
          AutofillClient::AddressPromptUserDecision::kMessageDeclined) {
    // |SaveAddressProfileInfobarBannerInteractionHandler::InfobarVisibilityChanged|
    // would be called even when the banner is explicitly dismissed by the
    // user. In that case, do not change the |user_decision_|.
    return;
  }
  if (user_decision_ ==
          AutofillClient::AddressPromptUserDecision::kEditAccepted ||
      user_decision_ == AutofillClient::AddressPromptUserDecision::kAccepted) {
    // The infobar has already been saved. So, cancel should not change the
    // |user_decision_| now.
    return;
  }

  DCHECK(user_decision_ != AutofillClient::AddressPromptUserDecision::kNever);
  user_decision_ = user_decision;
}

}  // namespace autofill
