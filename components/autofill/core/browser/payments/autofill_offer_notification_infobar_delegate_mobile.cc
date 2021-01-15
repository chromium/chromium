// Copyright 2021 The Chromium Authors. All rights reserved.
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
    AutofillOfferNotificationInfoBarDelegateMobile(const CreditCard& card)
    : credit_card_identifier_string_(
          card.CardIdentifierStringForAutofillDisplay()),
      network_icon_id_(CreditCard::IconResourceId(card.network())) {}

AutofillOfferNotificationInfoBarDelegateMobile::
    ~AutofillOfferNotificationInfoBarDelegateMobile() {}

void AutofillOfferNotificationInfoBarDelegateMobile::OnOfferDeepLinkClicked(
    GURL url) {
  infobar()->owner()->OpenURL(url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

int AutofillOfferNotificationInfoBarDelegateMobile::GetIconId() const {
  return IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER;
}

base::string16 AutofillOfferNotificationInfoBarDelegateMobile::GetMessageText()
    const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_REMINDER_TITLE);
}

infobars::InfoBarDelegate::InfoBarIdentifier
AutofillOfferNotificationInfoBarDelegateMobile::GetIdentifier() const {
  return AUTOFILL_OFFER_NOTIFICATION_INFOBAR_DELEGATE;
}

int AutofillOfferNotificationInfoBarDelegateMobile::GetButtons() const {
  return BUTTON_OK;
}

base::string16 AutofillOfferNotificationInfoBarDelegateMobile::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_OFFERS_REMINDER_POSITIVE_BUTTON_LABEL);
  }

  NOTREACHED() << "Unsupported button label requested: " << button;
  return base::string16();
}

}  // namespace autofill
