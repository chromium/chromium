// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"

#include <unordered_map>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
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

void LogOffersSuggestionsPopupShown(bool first_time_being_logged) {
  if (first_time_being_logged) {
    // We log that the offers suggestions popup was shown once for this field
    // while autofilling if it is the first time being logged.
    base::UmaHistogramEnumeration(
        "Autofill.Offer.SuggestionsPopupShown",
        autofill::autofill_metrics::OffersSuggestionsPopupEvent::
            kOffersSuggestionsPopupShownOnce);
  }

  // We log every time the offers suggestions popup is shown, regardless if the
  // user is repeatedly clicking the same field.
  base::UmaHistogramEnumeration(
      "Autofill.Offer.SuggestionsPopupShown",
      autofill::autofill_metrics::OffersSuggestionsPopupEvent::
          kOffersSuggestionsPopupShown);
}

void LogIndividualOfferSuggestionEvent(
    OffersSuggestionsEvent event,
    AutofillOfferData::OfferType offer_type) {
  std::string histogram_name = "Autofill.Offer.Suggestion";

  // Switch to different sub-histogram depending on offer type being displayed.
  switch (offer_type) {
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      histogram_name += ".GPayPromoCodeOffer";
      break;
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return;
  }

  base::UmaHistogramEnumeration(histogram_name, event);
}

}  // namespace autofill::autofill_metrics
