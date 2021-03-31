// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_save_address_profile_delegate_ios.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/grit/components_scaled_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillSaveAddressProfileDelegateIOS::AutofillSaveAddressProfileDelegateIOS(
    const AutofillProfile& profile,
    AutofillClient::AddressProfileSavePromptCallback callback)
    : profile_(profile),
      address_profile_save_prompt_callback_(std::move(callback)) {}

AutofillSaveAddressProfileDelegateIOS::
    ~AutofillSaveAddressProfileDelegateIOS() = default;

// static
AutofillSaveAddressProfileDelegateIOS*
AutofillSaveAddressProfileDelegateIOS::FromInfobarDelegate(
    infobars::InfoBarDelegate* delegate) {
  return delegate->GetIdentifier() ==
                 AUTOFILL_ADDRESS_PROFILE_INFOBAR_DELEGATE_IOS
             ? static_cast<AutofillSaveAddressProfileDelegateIOS*>(delegate)
             : nullptr;
}

std::u16string
AutofillSaveAddressProfileDelegateIOS::GetMessageDescriptionText() const {
  // TODO(crbug.com/1167062): Replace with proper localized string.
  return std::u16string(u"Fill forms faster in Chrome");
}

std::u16string AutofillSaveAddressProfileDelegateIOS::GetMessageActionText()
    const {
  // TODO(crbug.com/1167062): Replace with proper localized string.
  return std::u16string(u"Save...");
}

const autofill::AutofillProfile*
AutofillSaveAddressProfileDelegateIOS::GetProfile() const {
  return &profile_;
}

bool AutofillSaveAddressProfileDelegateIOS::Accept() {
  RunSaveAddressProfilePromptCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted);
  return true;
}

void AutofillSaveAddressProfileDelegateIOS::InfoBarDismissed() {
  // If the address profile modal dialog is presented, the user will be asked to
  // save or cancel the address profile. In case the user cancels, then
  // InfoBarDismissed() will be called.
  if (modal_is_shown_ && !modal_is_dismissed_)
    return;

  RunSaveAddressProfilePromptCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
}

bool AutofillSaveAddressProfileDelegateIOS::Cancel() {
  RunSaveAddressProfilePromptCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined);
  return true;
}

int AutofillSaveAddressProfileDelegateIOS::GetIconId() const {
  // TODO(crbug.com/1167062): Replace with proper icon.
  return IDR_INFOBAR_AUTOFILL_CC;
}

std::u16string AutofillSaveAddressProfileDelegateIOS::GetMessageText() const {
  // TODO(crbug.com/1167062): Replace with proper localized string.
  return std::u16string(u"Save address?");
}

infobars::InfoBarDelegate::InfoBarIdentifier
AutofillSaveAddressProfileDelegateIOS::GetIdentifier() const {
  return AUTOFILL_ADDRESS_PROFILE_INFOBAR_DELEGATE_IOS;
}

bool AutofillSaveAddressProfileDelegateIOS::ShouldExpire(
    const NavigationDetails& details) const {
  // Expire the Infobar unless the navigation was triggered by the form that
  // presented the Infobar, or the navigation is a redirect.
  return !details.is_form_submission && !details.is_redirect;
}

int AutofillSaveAddressProfileDelegateIOS::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::u16string AutofillSaveAddressProfileDelegateIOS::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    // TODO(crbug.com/1167062): Replace with proper localized string.
    return std::u16string(u"Save");
  }

  if (button == BUTTON_CANCEL) {
    // TODO(crbug.com/1167062): Replace with proper localized string.
    return std::u16string(u"No Thanks");
  }

  NOTREACHED() << "Unsupported button label requested.";
  return std::u16string();
}

void AutofillSaveAddressProfileDelegateIOS::RunSaveAddressProfilePromptCallback(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  std::move(address_profile_save_prompt_callback_).Run(decision, profile_);

  // Reset the modal dialog flags.
  modal_is_shown_ = false;
  modal_is_dismissed_ = false;
}

}  // namespace autofill
