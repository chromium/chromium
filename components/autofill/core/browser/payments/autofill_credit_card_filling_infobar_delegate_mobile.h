// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_DELEGATE_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_DELEGATE_MOBILE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace autofill {

class CreditCard;

// An InfoBarDelegate that enables the user to allow or deny filling credit
// card information on a website form. Only used on mobile.
class AutofillCreditCardFillingInfoBarDelegateMobile
    : public ConfirmInfoBarDelegate {
 public:
  AutofillCreditCardFillingInfoBarDelegateMobile(
      const CreditCard& card,
      base::OnceClosure card_filling_callback);

  AutofillCreditCardFillingInfoBarDelegateMobile(
      const AutofillCreditCardFillingInfoBarDelegateMobile&) = delete;
  AutofillCreditCardFillingInfoBarDelegateMobile& operator=(
      const AutofillCreditCardFillingInfoBarDelegateMobile&) = delete;

  ~AutofillCreditCardFillingInfoBarDelegateMobile() override;

  int issuer_icon_id() const { return issuer_icon_id_; }
  const std::u16string& card_label() const { return card_label_; }
  const std::u16string& card_sub_label() const { return card_sub_label_; }
  void set_was_shown() { was_shown_ = true; }

  // ConfirmInfoBarDelegate (publicly exposed):
  int GetIconId() const override;
  std::u16string GetMessageText() const override;
  void InfoBarDismissed() override;
  bool Accept() override;
  bool Cancel() override;

 private:
  // ConfirmInfoBarDelegate (continued):
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;

  void LogUserAction(AutofillMetrics::InfoBarMetric user_action);

  // The callback after having accepted the infobar; will initiate filling the
  // credit card information.
  base::OnceClosure card_filling_callback_;

  // Did the user ever explicitly accept or dismiss this infobar?
  bool had_user_interaction_;

  // Tracks whether the infobar was shown.
  bool was_shown_;

  // The resource ID for the icon that identifies the issuer of the card.
  int issuer_icon_id_;

  std::u16string card_label_;
  std::u16string card_sub_label_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_DELEGATE_MOBILE_H_
