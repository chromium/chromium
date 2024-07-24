// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_offer_data.h"

#include <algorithm>

#include "base/ranges/algorithm.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

// TODO(crbug.com/40932427): Refactor these methods and create separate
// constructors that are specific to each offer.
// static
AutofillOfferData AutofillOfferData::GPayCardLinkedOffer(
    int64_t offer_id,
    base::Time expiry,
    const std::vector<GURL>& merchant_origins,
    const GURL& offer_details_url,
    const DisplayStrings& display_strings,
    const std::vector<int64_t>& eligible_instrument_id,
    const std::string& offer_reward_amount) {
  return AutofillOfferData(offer_id, expiry, merchant_origins,
                           offer_details_url, display_strings,
                           eligible_instrument_id, offer_reward_amount);
}

// static
// TODO(b/351080010): DEPRECATED, remove this function.
AutofillOfferData AutofillOfferData::FreeListingCouponOffer(
    int64_t offer_id,
    base::Time expiry,
    const std::vector<GURL>& merchant_origins,
    const GURL& offer_details_url,
    const DisplayStrings& display_strings,
    const std::string& promo_code,
    bool is_merchant_wide,
    std::optional<std::string> terms_and_conditions) {
  return AutofillOfferData(OfferType::FREE_LISTING_COUPON_OFFER, offer_id,
                           expiry, merchant_origins, offer_details_url,
                           display_strings, promo_code);
}

// static
AutofillOfferData AutofillOfferData::GPayPromoCodeOffer(
    int64_t offer_id,
    base::Time expiry,
    const std::vector<GURL>& merchant_origins,
    const GURL& offer_details_url,
    const DisplayStrings& display_strings,
    const std::string& promo_code) {
  return AutofillOfferData(OfferType::GPAY_PROMO_CODE_OFFER, offer_id, expiry,
                           merchant_origins, offer_details_url, display_strings,
                           promo_code);
}

AutofillOfferData::AutofillOfferData() = default;

AutofillOfferData::~AutofillOfferData() = default;

AutofillOfferData::AutofillOfferData(const AutofillOfferData&) = default;

AutofillOfferData& AutofillOfferData::operator=(const AutofillOfferData&) =
    default;

bool AutofillOfferData::operator==(
    const AutofillOfferData& other_offer_data) const {
  return Compare(other_offer_data) == 0;
}

int AutofillOfferData::Compare(
    const AutofillOfferData& other_offer_data) const {
  int comparison = offer_id_ - other_offer_data.offer_id_;
  if (comparison != 0)
    return comparison;

  comparison =
      offer_reward_amount_.compare(other_offer_data.offer_reward_amount_);
  if (comparison != 0)
    return comparison;

  if (expiry_ < other_offer_data.expiry_)
    return -1;
  if (expiry_ > other_offer_data.expiry_)
    return 1;

  comparison = offer_details_url_.spec().compare(
      other_offer_data.offer_details_url_.spec());
  if (comparison != 0)
    return comparison;

  std::vector<GURL> merchant_origins_copy = merchant_origins_;
  std::vector<GURL> other_merchant_origins_copy =
      other_offer_data.merchant_origins_;
  std::sort(merchant_origins_copy.begin(), merchant_origins_copy.end());
  std::sort(other_merchant_origins_copy.begin(),
            other_merchant_origins_copy.end());
  if (merchant_origins_copy < other_merchant_origins_copy)
    return -1;
  if (merchant_origins_copy > other_merchant_origins_copy)
    return 1;

  std::vector<int64_t> eligible_instrument_id_copy = eligible_instrument_id_;
  std::vector<int64_t> other_eligible_instrument_id_copy =
      other_offer_data.eligible_instrument_id_;
  std::sort(eligible_instrument_id_copy.begin(),
            eligible_instrument_id_copy.end());
  std::sort(other_eligible_instrument_id_copy.begin(),
            other_eligible_instrument_id_copy.end());
  if (eligible_instrument_id_copy < other_eligible_instrument_id_copy)
    return -1;
  if (eligible_instrument_id_copy > other_eligible_instrument_id_copy)
    return 1;

  comparison = promo_code_.compare(other_offer_data.promo_code_);
  if (comparison != 0)
    return comparison;

  comparison = display_strings_.value_prop_text.compare(
      other_offer_data.display_strings_.value_prop_text);
  if (comparison != 0)
    return comparison;

  comparison = display_strings_.see_details_text.compare(
      other_offer_data.display_strings_.see_details_text);
  if (comparison != 0)
    return comparison;

  comparison = display_strings_.usage_instructions_text.compare(
      other_offer_data.display_strings_.usage_instructions_text);
  if (comparison != 0)
    return comparison;

  return 0;
}

bool AutofillOfferData::IsCardLinkedOffer() const {
  return GetOfferType() == OfferType::GPAY_CARD_LINKED_OFFER;
}

bool AutofillOfferData::IsPromoCodeOffer() const {
  return GetOfferType() == OfferType::GPAY_PROMO_CODE_OFFER;
}

bool AutofillOfferData::IsGPayPromoCodeOffer() const {
  return GetOfferType() == OfferType::GPAY_PROMO_CODE_OFFER;
}

bool AutofillOfferData::IsActiveAndEligibleForOrigin(const GURL& origin) const {
  return expiry_ > AutofillClock::Now() &&
         base::ranges::count(merchant_origins_, origin) > 0;
}

AutofillOfferData::AutofillOfferData(
    int64_t offer_id,
    base::Time expiry,
    const std::vector<GURL>& merchant_origins,
    const GURL& offer_details_url,
    const DisplayStrings& display_strings,
    const std::vector<int64_t>& eligible_instrument_id,
    const std::string& offer_reward_amount)
    : offer_type_(OfferType::GPAY_CARD_LINKED_OFFER),
      offer_id_(offer_id),
      expiry_(expiry),
      offer_details_url_(offer_details_url),
      merchant_origins_(merchant_origins),
      display_strings_(display_strings),
      offer_reward_amount_(offer_reward_amount),
      eligible_instrument_id_(eligible_instrument_id) {}

AutofillOfferData::AutofillOfferData(OfferType offer_type,
                                     int64_t offer_id,
                                     base::Time expiry,
                                     const std::vector<GURL>& merchant_origins,
                                     const GURL& offer_details_url,
                                     const DisplayStrings& display_strings,
                                     const std::string& promo_code)
    : offer_type_(offer_type),
      offer_id_(offer_id),
      expiry_(expiry),
      offer_details_url_(offer_details_url),
      merchant_origins_(merchant_origins),
      display_strings_(display_strings),
      promo_code_(promo_code) {}

}  // namespace autofill
