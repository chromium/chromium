// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"

#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/common/autofill_constants.h"
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
        const std::string& locale,
        AutofillClient::AddressProfileSavePromptCallback callback)
    : locale_(locale),
      profile_(profile),
      original_profile_(base::OptionalFromPtr(original_profile)),
      address_profile_save_prompt_callback_(std::move(callback)) {}

AutofillSaveUpdateAddressProfileDelegateIOS::
    ~AutofillSaveUpdateAddressProfileDelegateIOS() {
  // If the user has navigated away without saving the modal, then the
  // |address_profile_save_prompt_callback_| is run here.
  if (!address_profile_save_prompt_callback_.is_null()) {
    modal_was_shown_ = false;
    InfoBarDismissed();
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

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::GetDescription()
    const {
  return GetProfileDescription(
      original_profile_ ? *original_profile_ : profile_, locale_,
      /*include_address_and_contacts=*/true);
}

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::GetSubtitle() {
  DCHECK(original_profile_);
  std::vector<ProfileValueDifference> differences =
      GetProfileDifferenceForUi(original_profile_.value(), profile_, locale_);
  bool address_updated =
      std::find_if(differences.begin(), differences.end(),
                   [](const ProfileValueDifference& diff) {
                     return diff.type == ADDRESS_HOME_ADDRESS;
                   }) != differences.end();
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

const autofill::AutofillProfile*
AutofillSaveUpdateAddressProfileDelegateIOS::GetProfile() const {
  return &profile_;
}

const autofill::AutofillProfile*
AutofillSaveUpdateAddressProfileDelegateIOS::GetOriginalProfile() const {
  return base::OptionalOrNullptr(original_profile_);
}

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::GetProfileInfo(
    ServerFieldType type) const {
  return profile_.GetInfo(type, locale_);
}

std::vector<ProfileValueDifference>
AutofillSaveUpdateAddressProfileDelegateIOS::GetProfileDiff() const {
  return GetProfileDifferenceForUi(*GetProfile(), *GetOriginalProfile(),
                                   locale_);
}

bool AutofillSaveUpdateAddressProfileDelegateIOS::EditAccepted() {
  RunSaveAddressProfilePromptCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted);
  return true;
}

void AutofillSaveUpdateAddressProfileDelegateIOS::SetProfileInfo(
    const ServerFieldType& type,
    const std::u16string& value) {
  // Since the country field is a text field, we should use SetInfo() to make
  // sure they get converted to country codes.
  if (type == autofill::ADDRESS_HOME_COUNTRY) {
    profile_.SetInfoWithVerificationStatus(
        type, value, locale_,
        autofill::structured_address::VerificationStatus::kUserVerified);
    return;
  }

  profile_.SetRawInfoWithVerificationStatus(
      type, value,
      autofill::structured_address::VerificationStatus::kUserVerified);
}

bool AutofillSaveUpdateAddressProfileDelegateIOS::Accept() {
  RunSaveAddressProfilePromptCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted);
  return true;
}

void AutofillSaveUpdateAddressProfileDelegateIOS::InfoBarDismissed() {
  // If the address profile modal dialog is presented, InfoBarDismissed is
  // called due to BannerVisibilityChanged.
  if (modal_was_shown_)
    return;

  RunSaveAddressProfilePromptCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
}

bool AutofillSaveUpdateAddressProfileDelegateIOS::Cancel() {
  RunSaveAddressProfilePromptCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
  return true;
}

int AutofillSaveUpdateAddressProfileDelegateIOS::GetIconId() const {
  // TODO(crbug.com/1167062): Replace with proper icon.
  return IDR_INFOBAR_AUTOFILL_CC;
}

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::GetMessageText()
    const {
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
  // Expire the Infobar unless the navigation was triggered by the form that
  // presented the Infobar, or the navigation is a redirect.
  return !details.is_form_submission && !details.is_redirect;
}

int AutofillSaveUpdateAddressProfileDelegateIOS::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::u16string AutofillSaveUpdateAddressProfileDelegateIOS::GetButtonLabel(
    InfoBarButton button) const {

  NOTREACHED() << "Unsupported button label requested.";
  return std::u16string();
}

void AutofillSaveUpdateAddressProfileDelegateIOS::
    RunSaveAddressProfilePromptCallback(
        AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  std::move(address_profile_save_prompt_callback_).Run(decision, profile_);

  // Reset the modal dialog flags.
  modal_was_shown_ = false;
}

}  // namespace autofill
