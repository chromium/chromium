// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_OFFER_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_OFFER_DATA_H_

#include <optional>
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
// promo codes. Merchants are determined by |merchant_origins|.
class AutofillOfferData {
 public:
  // The specific type of offer.
  enum class OfferType {
    // Default value, should not be used.
    UNKNOWN,
    // GPay-activated card linked offer.
    GPAY_CARD_LINKED_OFFER,
    // GPay-activated promo code offer.
    GPAY_PROMO_CODE_OFFER,
    // TODO(b/351080010): Remove this type.
    // Promo code offer from the FreeListingCouponService. DEPRECATED.
    FREE_LISTING_COUPON_OFFER,
  };

  // Returns an AutofillOfferData for a GPay card-linked offer.
  static AutofillOfferData GPayCardLinkedOffer(
      int64_t offer_id,
      base::Time expiry,
      const std::vector<GURL>& merchant_origins,
      const GURL& offer_details_url,
      const DisplayStrings& display_strings,
      const std::vector<int64_t>& eligible_instrument_id,
      const std::string& offer_reward_amount);
  // TODO(b/351080010): remove this function.
  // Returns an AutofillOfferData for a free-listing coupon offer.
  static AutofillOfferData FreeListingCouponOffer(
      int64_t offer_id,
      base::Time expiry,
      const std::vector<GURL>& merchant_origins,
      const GURL& offer_details_url,
      const DisplayStrings& display_strings,
      const std::string& promo_code,
      bool is_merchant_wide = false,
      std::optional<std::string> terms_and_conditions = std::nullopt);
  // Returns an AutofillOfferData for a GPay promo code offer.
  static AutofillOfferData GPayPromoCodeOffer(
      int64_t offer_id,
      base::Time expiry,
      const std::vector<GURL>& merchant_origins,
      const GURL& offer_details_url,
      const DisplayStrings& display_strings,
      const std::string& promo_code);

  // TODO(crbug.com/40932427): Refactor this class to ensure the correct access
  // specifiers and move constructors and move assignment constructors.
  AutofillOfferData();
  ~AutofillOfferData();
  AutofillOfferData(const AutofillOfferData&);
  AutofillOfferData& operator=(const AutofillOfferData&);
  bool operator==(const AutofillOfferData& other_offer_data) const;

  // Compares two AutofillOfferData based on their member fields. Returns 0 if
  // the two offer data are exactly same. Otherwise returns the comparison
  // result of first found difference.
  int Compare(const AutofillOfferData& other_offer_data) const;

  // Returns true if the current offer is a card-linked offer.
  bool IsCardLinkedOffer() const;

  // Returns true if the current offer is a GPay promo code offer or an offer
  // from the FreeListingCouponService.
  bool IsPromoCodeOffer() const;

  // Returns true if the current offer is a GPay promo code offer.
  bool IsGPayPromoCodeOffer() const;

  // Returns true if the current offer is 1) not expired and 2) contains the
  // given |origin| in the list of |merchant_origins|.
  bool IsActiveAndEligibleForOrigin(const GURL& origin) const;

  OfferType GetOfferType() const { return offer_type_; }
  int64_t GetOfferId() const { return offer_id_; }
  base::Time GetExpiry() const { return expiry_; }
  const std::vector<GURL>& GetMerchantOrigins() const {
    return merchant_origins_;
  }
  const GURL& GetOfferDetailsUrl() const { return offer_details_url_; }
  const DisplayStrings& GetDisplayStrings() const { return display_strings_; }
  const std::string& GetOfferRewardAmount() const {
    return offer_reward_amount_;
  }
  const std::vector<int64_t>& GetEligibleInstrumentIds() const {
    return eligible_instrument_id_;
  }
  const std::string& GetPromoCode() const { return promo_code_; }

#ifdef UNIT_TEST
  void SetOfferIdForTesting(int64_t offer_id) { offer_id_ = offer_id; }
  void SetMerchantOriginForTesting(const std::vector<GURL>& merchant_origins) {
    merchant_origins_ = merchant_origins;
  }
  void SetEligibleInstrumentIdForTesting(
      const std::vector<int64_t>& eligible_instrument_id) {
    eligible_instrument_id_ = eligible_instrument_id;
  }

  void SetPromoCode(const std::string& promo_code) { promo_code_ = promo_code; }

  void SetValuePropTextInDisplayStrings(const std::string& value_prop_text) {
    display_strings_.value_prop_text = value_prop_text;
  }

  void SetOfferDetailsUrl(const GURL& offer_details_url) {
    offer_details_url_ = offer_details_url;
  }
#endif

 private:
  // Constructs an AutofillOfferData for a card-linked offer.
  AutofillOfferData(int64_t offer_id,
                    base::Time expiry,
                    const std::vector<GURL>& merchant_origins,
                    const GURL& offer_details_url,
                    const DisplayStrings& display_strings,
                    const std::vector<int64_t>& eligible_instrument_id,
                    const std::string& offer_reward_amount);
  // Constructs an AutofillOfferData for a promo code offer (GPay).
  AutofillOfferData(OfferType offer_type,
                    int64_t offer_id,
                    base::Time expiry,
                    const std::vector<GURL>& merchant_origins,
                    const GURL& offer_details_url,
                    const DisplayStrings& display_strings,
                    const std::string& promo_code);

  // The specific type of offer, which informs decisions made by other classes,
  // such as UI rendering or metrics.
  OfferType offer_type_ = OfferType::UNKNOWN;

  // The unique server ID for this offer data.
  int64_t offer_id_;

  // The timestamp when the offer will expire. Expired offers will not be shown
  // in the frontend.
  base::Time expiry_;

  // The URL that contains the offer details.
  GURL offer_details_url_;

  // The merchants' URL origins (path not included) where this offer can be
  // redeemed.
  std::vector<GURL> merchant_origins_;

  // Optional server-driven strings for certain offer elements. Generally most
  // useful for promo code offers, but could potentially apply to card-linked
  // offers as well.
  DisplayStrings display_strings_;

  /* Card-linked offer-specific fields */

  // The string including the reward details of the offer. Could be either
  // percentage off (XXX%) or fixed amount off ($XXX).
  std::string offer_reward_amount_;

  // The ids of the cards this offer can be applied to.
  std::vector<int64_t> eligible_instrument_id_;

  /* Promo code offer-specific fields */

  // A promo/gift/coupon code that can be applied at checkout with the merchant.
  std::string promo_code_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_OFFER_DATA_H_
