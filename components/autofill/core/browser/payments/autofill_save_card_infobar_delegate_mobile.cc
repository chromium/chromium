// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"

#include <utility>

#include "base/notreached.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace autofill {

AutofillSaveCardInfoBarDelegateMobile::AutofillSaveCardInfoBarDelegateMobile(
    AutofillSaveCardUiInfo ui_info,
    std::unique_ptr<AutofillSaveCardDelegate> common_delegate)
    : ui_info_(std::move(ui_info)), delegate_(std::move(common_delegate)) {
  delegate_->OnUiShown();
}

AutofillSaveCardInfoBarDelegateMobile::
    ~AutofillSaveCardInfoBarDelegateMobile() {
  delegate_->OnUiIgnored();
}

// static
AutofillSaveCardInfoBarDelegateMobile*
AutofillSaveCardInfoBarDelegateMobile::FromInfobarDelegate(
    infobars::InfoBarDelegate* delegate) {
  return delegate->GetIdentifier() == AUTOFILL_CC_INFOBAR_DELEGATE_MOBILE
             ? static_cast<AutofillSaveCardInfoBarDelegateMobile*>(delegate)
             : nullptr;
}

void AutofillSaveCardInfoBarDelegateMobile::OnLegalMessageLinkClicked(
    GURL url) {
  infobar()->owner()->OpenURL(url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

bool AutofillSaveCardInfoBarDelegateMobile::IsGooglePayBrandingEnabled() const {
  return ui_info_.is_google_pay_branding_enabled;
}

std::u16string AutofillSaveCardInfoBarDelegateMobile::GetDescriptionText()
    const {
  return ui_info_.description_text;
}

int AutofillSaveCardInfoBarDelegateMobile::GetIconId() const {
  return ui_info_.logo_icon_id;
}

std::u16string AutofillSaveCardInfoBarDelegateMobile::GetMessageText() const {
  return ui_info_.title_text;
}

infobars::InfoBarDelegate::InfoBarIdentifier
AutofillSaveCardInfoBarDelegateMobile::GetIdentifier() const {
  return AUTOFILL_CC_INFOBAR_DELEGATE_MOBILE;
}

bool AutofillSaveCardInfoBarDelegateMobile::ShouldExpire(
    const NavigationDetails& details) const {
#if BUILDFLAG(IS_IOS)
  // Expire the Infobar unless the navigation was triggered by the form that
  // presented the Infobar, or the navigation is a redirect.
  return !details.is_form_submission && !details.is_redirect;
#else   // BUILDFLAG(IS_IOS)
  // The user has submitted a form, causing the page to navigate elsewhere. We
  // don't want the infobar to be expired at this point, because the user won't
  // get a chance to answer the question.
  return false;
#endif  // BUILDFLAG(IS_IOS)
}

void AutofillSaveCardInfoBarDelegateMobile::InfoBarDismissed() {
  delegate_->OnUiCanceled();
}

bool AutofillSaveCardInfoBarDelegateMobile::Cancel() {
  delegate_->OnUiCanceled();
  return true;
}

int AutofillSaveCardInfoBarDelegateMobile::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::u16string AutofillSaveCardInfoBarDelegateMobile::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return ui_info_.confirm_text;
  }

  if (button == BUTTON_CANCEL) {
    return ui_info_.cancel_text;
  }

  NOTREACHED() << "Unsupported button label requested.";
  return std::u16string();
}

bool AutofillSaveCardInfoBarDelegateMobile::Accept() {
#if BUILDFLAG(IS_ANDROID)
  delegate_->OnUiAccepted(
      base::BindOnce(&AutofillSaveCardInfoBarDelegateMobile::RemoveInfobar,
                     base::Unretained(this)));
  return false;
#else
  delegate_->OnUiAccepted();
  return true;
#endif
}

#if BUILDFLAG(IS_IOS)
bool AutofillSaveCardInfoBarDelegateMobile::UpdateAndAccept(
    std::u16string cardholder_name,
    std::u16string expiration_date_month,
    std::u16string expiration_date_year) {
  AutofillClient::UserProvidedCardDetails user_provided_details;
  user_provided_details.cardholder_name = cardholder_name;
  user_provided_details.expiration_date_month = expiration_date_month;
  user_provided_details.expiration_date_year = expiration_date_year;
  delegate_->OnUiUpdatedAndAccepted(user_provided_details);
  return true;
}
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
void AutofillSaveCardInfoBarDelegateMobile::RemoveInfobar() {
  if (infobar()) {
    infobar()->RemoveSelf();
  }
}
#endif

}  // namespace autofill
