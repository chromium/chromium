// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_OFFER_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_OFFER_DATA_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace autofill {

// Represents an offer for certain merchants redeemable with certain cards.
// Merchants are determined by |merchant_domain| and the unique ids of cards are
// stored in |eligible_instrument_id|.
struct AutofillOfferData {
 public:
  AutofillOfferData();
  ~AutofillOfferData();
  AutofillOfferData(const AutofillOfferData&);
  AutofillOfferData& operator=(const AutofillOfferData&);
  bool operator==(const AutofillOfferData& other_offer_data) const;
  bool operator!=(const AutofillOfferData& other_offer_data) const;

  // Compares two AutofillOfferData based on their member fields. Returns 0 if
  // the two offer data are exactly same. Otherwise returns the comparison
  // result of first found difference.
  int Compare(const AutofillOfferData& other_offer_data) const;

  // The unique server ID for this offer data.
  int64_t offer_id;

  // The string including the reward details of the offer. Could be either
  // percentage off (XXX%) or fixed amount off ($XXX).
  std::string offer_reward_amount;

  // The timestamp when the offer will expire. Expired offers will not be shown
  // in the frontend.
  base::Time expiry;

  // The URL that contains the offer details.
  GURL offer_details_url;

  // The merchants' URLs where this offer can be redeemed.
  std::vector<GURL> merchant_domain;

  // The ids of the cards this offer can be applied to.
  std::vector<int64_t> eligible_instrument_id;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_OFFER_DATA_H_
