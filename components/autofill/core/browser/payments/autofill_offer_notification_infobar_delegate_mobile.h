// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_NOTIFICATION_INFOBAR_DELEGATE_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_NOTIFICATION_INFOBAR_DELEGATE_MOBILE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace autofill {

class CreditCard;

// An InfoBarDelegate that provides the information required to display an
// InfoBar when an offer is available on the merchant website.
class AutofillOfferNotificationInfoBarDelegateMobile
    : public ConfirmInfoBarDelegate {
 public:
  AutofillOfferNotificationInfoBarDelegateMobile(const GURL& offer_details_url,
                                                 const CreditCard& card);

  ~AutofillOfferNotificationInfoBarDelegateMobile() override;

  AutofillOfferNotificationInfoBarDelegateMobile(
      const AutofillOfferNotificationInfoBarDelegateMobile&) = delete;
  AutofillOfferNotificationInfoBarDelegateMobile& operator=(
      const AutofillOfferNotificationInfoBarDelegateMobile&) = delete;

  const std::u16string& credit_card_identifier_string() const {
    return credit_card_identifier_string_;
  }
  int network_icon_id() { return network_icon_id_; }
  const GURL& deep_link_url() const { return deep_link_url_; }

  // Called when the offer details deep link was clicked.
  virtual void OnOfferDeepLinkClicked(GURL url);

  // ConfirmInfoBarDelegate:
  int GetIconId() const override;
  std::u16string GetMessageText() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  void InfoBarDismissed() override;
  bool Accept() override;

 private:
  // Identifier for the credit card associated with the offer.
  std::u16string credit_card_identifier_string_;
  // Resource id for the icon representing the network of the credit card.
  int network_icon_id_;
  // URL that links to the offer details page in the Google Pay app.
  GURL deep_link_url_;
  // Indicates whether the user manually closed the infobar by clicking on the X
  // icon or the Got it button.
  bool user_manually_closed_infobar_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_NOTIFICATION_INFOBAR_DELEGATE_MOBILE_H_
