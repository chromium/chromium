// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_offer_data.h"

#include <algorithm>

namespace autofill {

AutofillOfferData::AutofillOfferData() = default;

AutofillOfferData::~AutofillOfferData() = default;

AutofillOfferData::AutofillOfferData(const AutofillOfferData&) = default;

AutofillOfferData& AutofillOfferData::operator=(const AutofillOfferData&) =
    default;

bool AutofillOfferData::operator==(
    const AutofillOfferData& other_offer_data) const {
  return Compare(other_offer_data) == 0;
}

bool AutofillOfferData::operator!=(
    const AutofillOfferData& other_offer_data) const {
  return Compare(other_offer_data) != 0;
}

int AutofillOfferData::Compare(
    const AutofillOfferData& other_offer_data) const {
  int comparison = offer_id - other_offer_data.offer_id;
  if (comparison != 0)
    return comparison;

  comparison =
      offer_reward_amount.compare(other_offer_data.offer_reward_amount);
  if (comparison != 0)
    return comparison;

  if (expiry < other_offer_data.expiry)
    return -1;
  if (expiry > other_offer_data.expiry)
    return 1;

  comparison = offer_details_url.spec().compare(
      other_offer_data.offer_details_url.spec());
  if (comparison != 0)
    return comparison;

  std::vector<GURL> merchant_domain_copy = merchant_domain;
  std::vector<GURL> other_merchant_domain_copy =
      other_offer_data.merchant_domain;
  std::sort(merchant_domain_copy.begin(), merchant_domain_copy.end());
  std::sort(other_merchant_domain_copy.begin(),
            other_merchant_domain_copy.end());
  if (merchant_domain_copy < other_merchant_domain_copy)
    return -1;
  if (merchant_domain_copy > other_merchant_domain_copy)
    return 1;

  std::vector<int64_t> eligible_instrument_id_copy = eligible_instrument_id;
  std::vector<int64_t> other_eligible_instrument_id_copy =
      other_offer_data.eligible_instrument_id;
  std::sort(eligible_instrument_id_copy.begin(),
            eligible_instrument_id_copy.end());
  std::sort(other_eligible_instrument_id_copy.begin(),
            other_eligible_instrument_id_copy.end());
  if (eligible_instrument_id_copy < other_eligible_instrument_id_copy)
    return -1;
  if (eligible_instrument_id_copy > other_eligible_instrument_id_copy)
    return 1;

  return 0;
}

}  // namespace autofill
