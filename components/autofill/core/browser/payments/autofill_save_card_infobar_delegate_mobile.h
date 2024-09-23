// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/gfx/image/image.h"

namespace autofill {

// An InfoBarDelegate that enables the user to allow or deny storing credit
// card information gathered from a form submission. Only used on mobile.
// This class adapts AutofillSaveCardUiInfo and AutofillSaveCardDelegate to the
// ConfirmInfoBarDelegate interface.
class AutofillSaveCardInfoBarDelegateMobile : public ConfirmInfoBarDelegate {
 public:
  // Creates a new delegate given the UI info and common delegate.
  AutofillSaveCardInfoBarDelegateMobile(
      AutofillSaveCardUiInfo ui_info,
      std::unique_ptr<AutofillSaveCardDelegate> common_delegate);

  AutofillSaveCardInfoBarDelegateMobile(
      const AutofillSaveCardInfoBarDelegateMobile&) = delete;
  AutofillSaveCardInfoBarDelegateMobile& operator=(
      const AutofillSaveCardInfoBarDelegateMobile&) = delete;

  ~AutofillSaveCardInfoBarDelegateMobile() override;

  // Returns |delegate| as an AutofillSaveCardInfoBarDelegateMobile, or nullptr
  // if it is of another type.
  static AutofillSaveCardInfoBarDelegateMobile* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  bool is_for_upload() const { return ui_info_.is_for_upload; }
  int issuer_icon_id() const { return ui_info_.issuer_icon_id; }
  const std::u16string& card_label() const { return ui_info_.card_label; }
  const std::u16string& card_sub_label() const {
    return ui_info_.card_sub_label;
  }
  const LegalMessageLines& legal_message_lines() const {
    return ui_info_.legal_message_lines;
  }
  const std::u16string& card_last_four_digits() const {
    return ui_info_.card_last_four_digits;
  }
  const std::u16string& cardholder_name() const {
    return ui_info_.cardholder_name;
  }
  const std::u16string& expiration_date_month() const {
    return ui_info_.expiration_date_month;
  }
  const std::u16string& expiration_date_year() const {
    return ui_info_.expiration_date_year;
  }
  const std::u16string& displayed_target_account_email() const {
    return ui_info_.displayed_target_account_email;
  }
  const gfx::Image& displayed_target_account_avatar() const {
    return ui_info_.displayed_target_account_avatar;
  }

  // Called when a link in the legal message text was clicked.
  virtual void OnLegalMessageLinkClicked(GURL url);

  // Google Pay branding is enabled with a flag and only for cards upstreamed
  // to Google.
  bool IsGooglePayBrandingEnabled() const;

  // Description text to be shown above the card information in the infobar.
  std::u16string GetDescriptionText() const;

  // ConfirmInfoBarDelegate:
  int GetIconId() const override;
  std::u16string GetMessageText() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

 protected:
  AutofillSaveCardDelegate* delegate() { return delegate_.get(); }

 private:
#if BUILDFLAG(IS_ANDROID)
  void RemoveInfobar();
#endif

  // Strings and assets provided to the info bar UI.
  AutofillSaveCardUiInfo ui_info_;
  // UI actions (accept, cancel, dismiss etc.) are forwarded to this object that
  // invokes callbacks and logs metrics.
  std::unique_ptr<AutofillSaveCardDelegate> delegate_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
