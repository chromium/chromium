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

// Server-driven strings for certain Offer UI elements.
struct DisplayStrings {
  // Explains the value of the offer. For example,
  // "5% off on shoes. Up to $50.".
  std::string value_prop_text;
  // A message implying or linking to additional details, usually "See details"
  // or "Terms apply", depending on the platform.
  std::string see_details_text;
  // Instructs the user on how they can redeem the offer, such as clicking into
  // a merchant's promo code field to trigger Autofill.
  std::string usage_instructions_text;
};

// Represents an offer for certain merchants. Card-linked offers are redeemable
// with certain cards, and the unique ids of those cards are stored in
// |eligible_instrument_id|. Promo code offers are redeemable with autofillable
// promo codes. Merchants are determined by |merchant_domain|.
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

  // Returns true if the current offer is a card-linked offer.
  bool IsCardLinkedOffer() const;

  // Returns true if the current offer is a promo code offer.
  bool IsPromoCodeOffer() const;

  // The unique server ID for this offer data.
  int64_t offer_id;

  // The timestamp when the offer will expire. Expired offers will not be shown
  // in the frontend.
  base::Time expiry;

  // The URL that contains the offer details.
  GURL offer_details_url;

  // The merchants' URLs where this offer can be redeemed.
  std::vector<GURL> merchant_domain;

  // Optional server-driven strings for certain offer elements. Generally most
  // useful for promo code offers, but could potentially apply to card-linked
  // offers as well.
  DisplayStrings display_strings;

  /* Card-linked offer-specific fields */

  // The string including the reward details of the offer. Could be either
  // percentage off (XXX%) or fixed amount off ($XXX).
  std::string offer_reward_amount;

  // The ids of the cards this offer can be applied to.
  std::vector<int64_t> eligible_instrument_id;

  /* Promo code offer-specific fields */

  // A promo/gift/coupon code that can be applied at checkout with the merchant.
  std::string promo_code;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_OFFER_DATA_H_
