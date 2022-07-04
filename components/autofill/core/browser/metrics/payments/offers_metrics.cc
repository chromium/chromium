// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"

#include <unordered_map>

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"

namespace autofill::autofill_metrics {

void LogStoredOfferMetrics(
    const std::vector<std::unique_ptr<AutofillOfferData>>& offers) {
  std::unordered_map<AutofillOfferData::OfferType, int> offer_count;
  for (const std::unique_ptr<AutofillOfferData>& offer : offers) {
    // This function should only be run when the profile is loaded, which means
    // the only offers that should be available are the ones that are stored on
    // disk. Since free listing coupons are not stored on disk, we should never
    // have any loaded here.
    DCHECK_NE(offer->GetOfferType(),
              AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER);

    offer_count[offer->GetOfferType()]++;

    if (offer->GetOfferType() ==
        AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER) {
      base::UmaHistogramCounts1000(
          "Autofill.Offer.StoredOfferRelatedMerchantCount",
          offer->GetMerchantOrigins().size());
      base::UmaHistogramCounts1000("Autofill.Offer.StoredOfferRelatedCardCount",
                                   offer->GetEligibleInstrumentIds().size());
    }
  }

  if (offer_count[AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER] > 0) {
    base::UmaHistogramCounts1000(
        "Autofill.Offer.StoredOfferCount.GPayPromoCodeOffer",
        offer_count[AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER]);
  }

  if (offer_count[AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER] > 0) {
    base::UmaHistogramCounts1000(
        "Autofill.Offer.StoredOfferCount.CardLinkedOffer",
        offer_count[AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER]);
  }
}

}  // namespace autofill::autofill_metrics
