// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_offer_manager.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/ranges.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
AutofillOfferData ToAutofillOfferData(
    const GURL& url,
    const commerce::DiscountInfo& discount_info) {
  return AutofillOfferData::FreeListingCouponOffer(
      discount_info.id, base::Time::FromDoubleT(discount_info.expiry_time_sec),
      {url}, url, DisplayStrings{discount_info.description_detail},
      discount_info.discount_code.value_or(""), discount_info.is_merchant_wide);
}

AutofillOfferManager::AutofillOfferManager(
    PersonalDataManager* personal_data,
    CouponServiceDelegate* coupon_service_delegate,
    std::unique_ptr<ShoppingServiceDelegate> shopping_service_delegate)
    : personal_data_(personal_data),
      coupon_service_delegate_(coupon_service_delegate),
      shopping_service_delegate_(std::move(shopping_service_delegate)) {
  personal_data_manager_observation.Observe(personal_data_);
  UpdateEligibleMerchantDomains();
}

AutofillOfferManager::~AutofillOfferManager() = default;

void AutofillOfferManager::OnPersonalDataChanged() {
  UpdateEligibleMerchantDomains();
}

void AutofillOfferManager::OnDidNavigateFrame(AutofillClient* client) {
  notification_handler_.UpdateOfferNotificationVisibility(client);
}

AutofillOfferManager::CardLinkedOffersMap
AutofillOfferManager::GetCardLinkedOffersMap(
    const GURL& last_committed_primary_main_frame_url) const {
  GURL last_committed_primary_main_frame_origin =
      last_committed_primary_main_frame_url.DeprecatedGetOriginAsURL();

  if (!base::Contains(eligible_merchant_domains_,
                      last_committed_primary_main_frame_origin)) {
    return {};
  }

  const std::vector<AutofillOfferData*> offers =
      personal_data_->GetAutofillOffers();
  const std::vector<CreditCard*> cards = personal_data_->GetCreditCards();
  AutofillOfferManager::CardLinkedOffersMap card_linked_offers_map;

  for (AutofillOfferData* offer : offers) {
    // Ensure the offer is valid.
    if (!offer->IsActiveAndEligibleForOrigin(
            last_committed_primary_main_frame_origin)) {
      continue;
    }

    // Ensure the offer is a card-linked offer.
    if (!offer->IsCardLinkedOffer()) {
      continue;
    }

    for (const CreditCard* card : cards) {
      // If card has an offer, add the card's guid id to the map. There is
      // currently a one-to-one mapping between cards and offer data.
      if (base::Contains(offer->GetEligibleInstrumentIds(),
                         card->instrument_id())) {
        card_linked_offers_map[card->guid()] = offer;
      }
    }
  }

  return card_linked_offers_map;
}

bool AutofillOfferManager::IsUrlEligible(
    const GURL& last_committed_primary_main_frame_url) {
  if (coupon_service_delegate_ && coupon_service_delegate_->IsUrlEligible(
                                      last_committed_primary_main_frame_url)) {
    return true;
  }
  return base::Contains(
      eligible_merchant_domains_,
      last_committed_primary_main_frame_url.DeprecatedGetOriginAsURL());
}

AutofillOfferData* AutofillOfferManager::GetOfferForUrl(
    const GURL& last_committed_primary_main_frame_url) {
  if (coupon_service_delegate_) {
    for (AutofillOfferData* offer :
         coupon_service_delegate_->GetFreeListingCouponsForUrl(
             last_committed_primary_main_frame_url)) {
      if (offer->IsActiveAndEligibleForOrigin(
              last_committed_primary_main_frame_url
                  .DeprecatedGetOriginAsURL())) {
        return offer;
      }
    }
  }

  for (AutofillOfferData* offer : personal_data_->GetAutofillOffers()) {
    if (offer->IsActiveAndEligibleForOrigin(
            last_committed_primary_main_frame_url.DeprecatedGetOriginAsURL())) {
      return offer;
    }
  }

  return nullptr;
}

void AutofillOfferManager::GetShoppingServiceOfferForUrl(
    const GURL& url,
    AsyncOfferCallback callback) {
  if ((shopping_service_delegate_ &&
       shopping_service_delegate_->IsDiscountEligibleToShowOnNavigation()) ||
      (base::FeatureList::IsEnabled(
          ntp_features::kNtpHistoryClustersModuleDiscounts))) {
    shopping_service_delegate_->GetDiscountInfoForUrls(
        {url}, base::BindOnce(
                   &AutofillOfferManager::HandleShoppingServiceResponse,
                   weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
  }
}

void AutofillOfferManager::UpdateEligibleMerchantDomains() {
  eligible_merchant_domains_.clear();
  std::vector<AutofillOfferData*> offers = personal_data_->GetAutofillOffers();

  for (auto* offer : offers) {
    eligible_merchant_domains_.insert(offer->GetMerchantOrigins().begin(),
                                      offer->GetMerchantOrigins().end());
  }
}

void AutofillOfferManager::HandleShoppingServiceResponse(
    const GURL& url,
    AsyncOfferCallback callback,
    const commerce::DiscountsMap& discounts) {
  if (discounts.empty()) {
    return;
  }

  CHECK(discounts.size() == 1 && discounts.count(url) == 1 &&
        discounts.at(url).size() > 0);

  std::move(callback).Run(url, ToAutofillOfferData(url, discounts.at(url)[0]));
}

}  // namespace autofill
