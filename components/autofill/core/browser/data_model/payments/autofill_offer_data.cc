// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"

#include <algorithm>
#include <string>
#include <vector>

#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "url/gurl.h"

namespace autofill {

AutofillOfferData::AutofillOfferData(std::string offer_id,
                                     base::Time expiry,
                                     std::vector<GURL> merchant_origins,
                                     GURL offer_details_url,
                                     DisplayStrings display_strings,
                                     std::string promo_code,
                                     std::string offer_reward_amount,
                                     base::Time issue_time)
    : offer_type_(OfferType::WALLET_DIRECT_OFFER),
      offer_id_(std::move(offer_id)),
      expiry_(expiry),
      issue_time_(issue_time),
      offer_details_url_(std::move(offer_details_url)),
      merchant_origins_(std::move(merchant_origins)),
      display_strings_(std::move(display_strings)),
      offer_reward_amount_(std::move(offer_reward_amount)),
      promo_code_(std::move(promo_code)) {}

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
  int comparison = offer_id_.compare(other_offer_data.offer_id_);
  if (comparison != 0) {
    return comparison;
  }

  comparison =
      offer_reward_amount_.compare(other_offer_data.offer_reward_amount_);
  if (comparison != 0) {
    return comparison;
  }

  if (expiry_ < other_offer_data.expiry_) {
    return -1;
  }
  if (expiry_ > other_offer_data.expiry_) {
    return 1;
  }

  if (issue_time_ < other_offer_data.issue_time_) {
    return -1;
  }
  if (issue_time_ > other_offer_data.issue_time_) {
    return 1;
  }

  comparison = offer_details_url_.spec().compare(
      other_offer_data.offer_details_url_.spec());
  if (comparison != 0) {
    return comparison;
  }

  std::vector<GURL> merchant_origins_copy = merchant_origins_;
  std::vector<GURL> other_merchant_origins_copy =
      other_offer_data.merchant_origins_;
  std::sort(merchant_origins_copy.begin(), merchant_origins_copy.end());
  std::sort(other_merchant_origins_copy.begin(),
            other_merchant_origins_copy.end());
  if (merchant_origins_copy < other_merchant_origins_copy) {
    return -1;
  }
  if (merchant_origins_copy > other_merchant_origins_copy) {
    return 1;
  }

  comparison = promo_code_.compare(other_offer_data.promo_code_);
  if (comparison != 0) {
    return comparison;
  }

  comparison = display_strings_.value_prop_text.compare(
      other_offer_data.display_strings_.value_prop_text);
  if (comparison != 0) {
    return comparison;
  }

  comparison = display_strings_.see_details_text.compare(
      other_offer_data.display_strings_.see_details_text);
  if (comparison != 0) {
    return comparison;
  }

  comparison = display_strings_.usage_instructions_text.compare(
      other_offer_data.display_strings_.usage_instructions_text);
  if (comparison != 0) {
    return comparison;
  }

  return 0;
}

bool AutofillOfferData::IsActiveAndEligibleForOrigin(const GURL& origin) const {
  return expiry_ > AutofillClock::Now() && IsEligibleForOrigin(origin);
}

bool AutofillOfferData::IsEligibleForOrigin(const GURL& origin) const {
  return std::ranges::any_of(
      merchant_origins_, [&origin](const GURL& merchant_origin) {
        return affiliations::IsExtendedPublicSuffixDomainMatch(merchant_origin,
                                                               origin, {});
      });
}

}  // namespace autofill
