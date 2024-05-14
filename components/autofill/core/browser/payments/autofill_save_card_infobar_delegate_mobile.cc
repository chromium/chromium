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
  // The user has submitted a form, causing the page to navigate elsewhere. We
  // don't want the infobar to be expired at this point, because the user won't
  // get a chance to answer the question.
  return false;
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

  NOTREACHED_IN_MIGRATION() << "Unsupported button label requested.";
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

#if BUILDFLAG(IS_ANDROID)
void AutofillSaveCardInfoBarDelegateMobile::RemoveInfobar() {
  if (infobar()) {
    infobar()->RemoveSelf();
  }
}
#endif

}  // namespace autofill
