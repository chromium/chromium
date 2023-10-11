// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/offer_notification_handler.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/search/ntp_features.h"
#include "url/gurl.h"

namespace autofill {

namespace {

bool IsOfferValid(AutofillOfferData* offer) {
  if (!offer) {
    return false;
  }

  if (offer->GetMerchantOrigins().empty()) {
    return false;
  }

  if (offer->IsPromoCodeOffer() &&
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableOfferNotificationForPromoCodes)) {
    return false;
  }

  if (offer->GetOfferType() == AutofillOfferData::OfferType::UNKNOWN) {
    return false;
  }

  return true;
}

bool ShouldAutoPopup(commerce::DiscountDialogAutoPopupBehavior behavior,
                     bool shown_before) {
  switch (behavior) {
    case commerce::DiscountDialogAutoPopupBehavior::kAutoPopupOnce:
      return !shown_before;
    case commerce::DiscountDialogAutoPopupBehavior::kAlwaysAutoPopup:
      return true;
    case commerce::DiscountDialogAutoPopupBehavior::kNoAutoPopup:
      return false;
  }
}

bool ShouldAutoPopupForHistoryClustersModuleDiscounts(bool shown_before) {
  auto behavior = static_cast<commerce::DiscountDialogAutoPopupBehavior>(
      commerce::kHistoryClustersBehavior.Get());
  return ShouldAutoPopup(behavior, shown_before);
}

bool ShouldAutoPopupForDiscounts(bool is_merchant_wide, bool shown_before) {
  auto behavior = is_merchant_wide
                      ? static_cast<commerce::DiscountDialogAutoPopupBehavior>(
                            commerce::kMerchantWideBehavior.Get())
                      : static_cast<commerce::DiscountDialogAutoPopupBehavior>(
                            commerce::kNonMerchantWideBehavior.Get());
  return ShouldAutoPopup(behavior, shown_before);
}

}  // namespace

OfferNotificationHandler::OfferNotificationHandler(
    AutofillOfferManager* offer_manager)
    : offer_manager_(offer_manager) {}

OfferNotificationHandler::~OfferNotificationHandler() = default;

void OfferNotificationHandler::UpdateOfferNotificationVisibility(
    AutofillClient* client) {
  const GURL url = client->GetLastCommittedPrimaryMainFrameURL();

  bool url_contains_discount_utm_tag =
      base::FeatureList::IsEnabled(
          ntp_features::kNtpHistoryClustersModuleDiscounts) &&
      commerce::UrlContainsDiscountUtmTag(url);
  bool valid_offer_exists_for_url_on_device = ValidOfferExistsForUrl(url);

  bool show_offer_notification_for_server_retrieved_offer = false;

  // Attempt to show an offer notification bubble to the user for offers that
  // are stored on the device that do not contain a discount UTM tag. These
  // offers are prioritized because they can instantly be shown, since the other
  // types of offers that would be shown must be retrieved from the server using
  // the `offer_manager_- >GetShoppingServiceOfferForUrl()` call below, which
  // can have a delay.
  if (valid_offer_exists_for_url_on_device && !url_contains_discount_utm_tag) {
    // Try to show offer notification when the last committed URL has the domain
    // that an offer is applicable for.
    // TODO(crbug.com/1203811): GetOfferForUrl needs to know whether to give
    //   precedence to card-linked offers or promo code offers. Eventually,
    //   promo code offers should take precedence if a bubble is shown.
    //   Currently, if a url has both types of offers and the promo code offer
    //   is selected, no bubble will end up being shown (due to not yet being
    //   implemented).
    AutofillOfferData* offer = offer_manager_->GetOfferForUrl(url);
    CHECK(IsOfferValid(offer));
    int64_t offer_id = offer->GetOfferId();
    bool offer_id_has_shown_before = shown_notification_ids_.contains(offer_id);
    client->UpdateOfferNotification(
        offer, {.notification_has_been_shown = offer_id_has_shown_before,
                .show_notification_automatically = !offer_id_has_shown_before});
    shown_notification_ids_.insert(offer_id);
  } else {
    client->DismissOfferNotification();

    // The prerequisites were not met to show an offer stored on the device.
    // This ensures showing server retrieved offers is attempted later.
    if (base::FeatureList::IsEnabled(commerce::kShowDiscountOnNavigation) ||
        url_contains_discount_utm_tag) {
      show_offer_notification_for_server_retrieved_offer = true;
    }
  }

  // We always need to call GetShoppingServiceOfferForUrl to get the offer from
  // backend and store locally, If we have shown an offer stored on the device
  // above, the offer will not be shown here. Otherwise the offer retrieved here
  // from the server is shown if kShowDiscountOnNavigation is enabled or
  // kNtpHistoryClustersModuleDiscounts is enabled with the discount UTM.
  auto shopping_service_callback =
      show_offer_notification_for_server_retrieved_offer
          ? base::BindOnce(&OfferNotificationHandler::
                               UpdateOfferNotificationForShoppingServiceOffer,
                           weak_ptr_factory_.GetWeakPtr(), client)
          : base::DoNothing();

  offer_manager_->GetShoppingServiceOfferForUrl(
      url, std::move(shopping_service_callback));
}

void OfferNotificationHandler::ClearShownNotificationIdForTesting() {
  shown_notification_ids_.clear();
}

void OfferNotificationHandler::AddShownNotificationIdForTesting(
    int64_t shown_notification_id) {
  shown_notification_ids_.insert(shown_notification_id);
}

bool OfferNotificationHandler::ValidOfferExistsForUrl(const GURL& url) {
  return offer_manager_->IsUrlEligible(url) &&
         IsOfferValid(offer_manager_->GetOfferForUrl(url));
}

void OfferNotificationHandler::UpdateOfferNotificationForShoppingServiceOffer(
    AutofillClient* client,
    const GURL& url,
    const AutofillOfferData& offer) {
  if (!client || url != client->GetLastCommittedPrimaryMainFrameURL()) {
    return;
  }
  int64_t offer_id = offer.GetOfferId();
  OfferNotificationOptions offer_notification_options = {
      .notification_has_been_shown = shown_notification_ids_.contains(offer_id),
      .expand_notification_icon = true,
      .show_notification_automatically =
          ShowShoppingServiceOfferNotificationAutomatically(url, offer)};

  client->UpdateOfferNotification(&offer, offer_notification_options);
  shown_notification_ids_.insert(offer_id);
}

bool OfferNotificationHandler::
    ShowShoppingServiceOfferNotificationAutomatically(
        const GURL& url,
        const AutofillOfferData& offer) {
  bool offer_has_been_shown_before =
      shown_notification_ids_.contains(offer.GetOfferId());

  // If the URL contains the expected UTM tags, the notification should show
  // automatically if the offer has not been shown before.
  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpHistoryClustersModuleDiscounts) &&
      commerce::UrlContainsDiscountUtmTag(url)) {
    return ShouldAutoPopupForHistoryClustersModuleDiscounts(
        offer_has_been_shown_before);
  }

  return ShouldAutoPopupForDiscounts(offer.IsMerchantWideOffer(),
                                     offer_has_been_shown_before);
}

}  // namespace autofill
