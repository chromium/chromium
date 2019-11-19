// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_credit_card_filling_infobar_delegate_mobile.h"

#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/grit/components_scaled_resources.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillCreditCardFillingInfoBarDelegateMobile::
    AutofillCreditCardFillingInfoBarDelegateMobile(
        const CreditCard& card,
        base::OnceClosure card_filling_callback)
    : ConfirmInfoBarDelegate(),
      card_filling_callback_(std::move(card_filling_callback)),
      had_user_interaction_(false),
      was_shown_(false),
      issuer_icon_id_(CreditCard::IconResourceId(card.network())),
#if defined(OS_IOS)
      card_label_(card.NetworkAndLastFourDigits()),
#else
      card_label_(base::string16(kMidlineEllipsis) + card.LastFourDigits()),
#endif
      card_sub_label_(card.AbbreviatedExpirationDateForDisplay(false)) {
}

AutofillCreditCardFillingInfoBarDelegateMobile::
    ~AutofillCreditCardFillingInfoBarDelegateMobile() {
  if (was_shown_) {
    AutofillMetrics::LogCreditCardFillingInfoBarMetric(
        AutofillMetrics::INFOBAR_SHOWN);
    if (!had_user_interaction_)
      LogUserAction(AutofillMetrics::INFOBAR_IGNORED);
  }
}

int AutofillCreditCardFillingInfoBarDelegateMobile::GetIconId() const {
  return IDR_INFOBAR_AUTOFILL_CC;
}

base::string16 AutofillCreditCardFillingInfoBarDelegateMobile::GetMessageText()
    const {
#if defined(OS_ANDROID)
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_TITLE);
#elif defined(OS_IOS)
  // On iOS the card details are in the title of the infobar.
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_FORMATTED_TITLE, card_label_);
#endif
}

void AutofillCreditCardFillingInfoBarDelegateMobile::InfoBarDismissed() {
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
}

bool AutofillCreditCardFillingInfoBarDelegateMobile::Accept() {
  std::move(card_filling_callback_).Run();
  LogUserAction(AutofillMetrics::INFOBAR_ACCEPTED);
  return true;
}

bool AutofillCreditCardFillingInfoBarDelegateMobile::Cancel() {
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
  return true;
}

infobars::InfoBarDelegate::InfoBarIdentifier
AutofillCreditCardFillingInfoBarDelegateMobile::GetIdentifier() const {
  return AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_DELEGATE_ANDROID;
}

base::string16 AutofillCreditCardFillingInfoBarDelegateMobile::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      button == BUTTON_OK ? IDS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_ACCEPT
                          : IDS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_NO_THANKS);
}

void AutofillCreditCardFillingInfoBarDelegateMobile::LogUserAction(
    AutofillMetrics::InfoBarMetric user_action) {
  DCHECK(!had_user_interaction_);

  AutofillMetrics::LogCreditCardFillingInfoBarMetric(user_action);
  had_user_interaction_ = true;
}

}  // namespace autofill
