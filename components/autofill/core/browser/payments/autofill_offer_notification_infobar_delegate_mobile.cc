// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_offer_notification_infobar_delegate_mobile.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/grit/components_scaled_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace autofill {

AutofillOfferNotificationInfoBarDelegateMobile::
    AutofillOfferNotificationInfoBarDelegateMobile(
        const GURL& offer_details_url,
        const CreditCard& card)
    : credit_card_identifier_string_(card.CardNameAndLastFourDigits()),
      network_icon_id_(CreditCard::IconResourceId(card.network())),
      deep_link_url_(offer_details_url),
      user_manually_closed_infobar_(false) {
  autofill_metrics::LogOfferNotificationInfoBarShown();
}

AutofillOfferNotificationInfoBarDelegateMobile::
    ~AutofillOfferNotificationInfoBarDelegateMobile() {
  if (!user_manually_closed_infobar_) {
    autofill_metrics::LogOfferNotificationInfoBarResultMetric(
        autofill_metrics::OfferNotificationInfoBarResultMetric::
            OFFER_NOTIFICATION_INFOBAR_IGNORED);
  }
}

void AutofillOfferNotificationInfoBarDelegateMobile::OnOfferDeepLinkClicked(
    GURL url) {
  autofill_metrics::LogOfferNotificationInfoBarDeepLinkClicked();
  infobar()->owner()->OpenURL(url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

int AutofillOfferNotificationInfoBarDelegateMobile::GetIconId() const {
  return IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER;
}

std::u16string AutofillOfferNotificationInfoBarDelegateMobile::GetMessageText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_LINKED_OFFER_REMINDER_TITLE);
}

infobars::InfoBarDelegate::InfoBarIdentifier
AutofillOfferNotificationInfoBarDelegateMobile::GetIdentifier() const {
  return AUTOFILL_OFFER_NOTIFICATION_INFOBAR_DELEGATE;
}

int AutofillOfferNotificationInfoBarDelegateMobile::GetButtons() const {
  return BUTTON_OK;
}

std::u16string AutofillOfferNotificationInfoBarDelegateMobile::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_OFFERS_REMINDER_POSITIVE_BUTTON_LABEL);
  }

  NOTREACHED() << "Unsupported button label requested: " << button;
  return std::u16string();
}

void AutofillOfferNotificationInfoBarDelegateMobile::InfoBarDismissed() {
  autofill_metrics::LogOfferNotificationInfoBarResultMetric(
      autofill_metrics::OfferNotificationInfoBarResultMetric::
          OFFER_NOTIFICATION_INFOBAR_CLOSED);
  user_manually_closed_infobar_ = true;
}

bool AutofillOfferNotificationInfoBarDelegateMobile::Accept() {
  autofill_metrics::LogOfferNotificationInfoBarResultMetric(
      autofill_metrics::OfferNotificationInfoBarResultMetric::
          OFFER_NOTIFICATION_INFOBAR_ACKNOWLEDGED);
  user_manually_closed_infobar_ = true;
  return true;
}

}  // namespace autofill
