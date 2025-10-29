// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PAYMENTS_BNPL_ISSUER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PAYMENTS_BNPL_ISSUER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/types/optional_ref.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/data_model/payments/payment_instrument.h"

namespace autofill {

// Contains information regarding a Buy Now Pay Later issuer that the user is
// eligible to use on certain merchant webpages.
class BnplIssuer {
 public:
  // Enum for Bnpl issuer id. Its values correspond to the Bnpl constants
  // defined in components/autofill/core/browser/payments/constants.h.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(IssuerId)
  enum class IssuerId {
    kBnplAffirm = 0,
    kBnplZip = 1,
    kBnplAfterpay = 2,
    kBnplKlarna = 3,
    kMaxValue = kBnplKlarna,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:BnplIssuerId)

  // Struct that links currency to the eligible price range in that currency for
  // a BNPL issuer.
  struct EligiblePriceRange {
    EligiblePriceRange(std::string currency,
                       int64_t price_lower_bound,
                       int64_t price_upper_bound)
        : currency(std::move(currency)),
          price_lower_bound(price_lower_bound),
          price_upper_bound(price_upper_bound) {}
    EligiblePriceRange(const EligiblePriceRange&);
    EligiblePriceRange& operator=(const EligiblePriceRange&);
    EligiblePriceRange(EligiblePriceRange&&);
    EligiblePriceRange& operator=(EligiblePriceRange&&);
    ~EligiblePriceRange();
    bool operator==(const EligiblePriceRange&) const;

    // Currency of the price range. This field contains a three-letter currency
    // code. e.g. "USD".
    std::string currency;

    // Lower bound for the US dollar price range that this BNPL provider
    // accepts, in micros of currency. A micro of currency is one millionths of
    // the base unit (dollars, not cents for example). e.g. $1.05 == 1050000.
    // Bound is inclusive.
    int64_t price_lower_bound;

    // Upper bound for the US dollar price range that this BNPL provider
    // accepts, in micros of currency. A micro of currency is one millionths of
    // the base unit (dollars, not cents for example). e.g. $1.05 == 1050000.
    // Bound is inclusive.
    int64_t price_upper_bound;
  };

  // The resource IDs for the light and dark mode BNPL issuer icons.
  using LightModeImageId = base::StrongAlias<class LightModeImageIdTag, int>;
  using DarkModeImageId = base::StrongAlias<class DarkModeImageIdTag, int>;

  BnplIssuer();
  // `instrument_id` is present for linked issuers, and nullopt for unlinked
  // issuers. `issuer_id` is the unique identifier of this specfiic issuer.
  // `eligible_price_ranges` is a list of currencies mapped to their price
  // ranges, in micros. 'action_required' is the additional steps needed to
  // use this issuer.
  BnplIssuer(std::optional<int64_t> instrument_id,
             IssuerId issuer_id,
             std::vector<EligiblePriceRange> eligible_price_ranges,
             DenseSet<PaymentInstrument::ActionRequired> action_required =
                 DenseSet<PaymentInstrument::ActionRequired>());
  BnplIssuer(const BnplIssuer&);
  BnplIssuer& operator=(const BnplIssuer&);
  BnplIssuer(BnplIssuer&&);
  BnplIssuer& operator=(BnplIssuer&&);
  ~BnplIssuer();
  friend bool operator==(const BnplIssuer&, const BnplIssuer&);

  IssuerId issuer_id() const { return issuer_id_; }
  void set_issuer_id(IssuerId issuer_id) { issuer_id_ = issuer_id; }

  const std::optional<PaymentInstrument>& payment_instrument() const {
    return payment_instrument_;
  }
  void set_payment_instrument(
      std::optional<PaymentInstrument> payment_instrument) {
    payment_instrument_ = payment_instrument;
  }

  base::span<const EligiblePriceRange> eligible_price_ranges() const
      LIFETIME_BOUND {
    return eligible_price_ranges_;
  }
  void set_eligible_price_ranges(
      std::vector<EligiblePriceRange> eligible_price_ranges) {
    eligible_price_ranges_ = std::move(eligible_price_ranges);
  }

  // Returns the eligible price range in `currency`.
  base::optional_ref<const EligiblePriceRange> GetEligiblePriceRangeForCurrency(
      const std::string& currency) const LIFETIME_BOUND;

  // Returns true if the given `amount_in_micros` is supported by this issuer in
  // the given `currency`.
  bool IsEligibleAmount(int64_t amount_in_micros,
                        const std::string& currency) const;

  // Returns the display name for the issuer when shown in UI.
  std::u16string GetDisplayName() const;

 private:
  // Unique identifier for the BNPL partner.
  IssuerId issuer_id_;

  // If the issuer is linked, `payment_instrument_` will contain the
  // instrument_id. If the issuer is unlinked, `payment_instrument_` will be
  // empty.
  std::optional<PaymentInstrument> payment_instrument_;

  // Vector of eligible price ranges for this BnplIssuer. Contains per-currency
  // eligible price ranges for BNPL, for all supported currencies.
  // TODO(crbug.com/393549948): Save eligible price ranges in map.
  std::vector<EligiblePriceRange> eligible_price_ranges_;
};

std::u16string BnplIssuerIdToDisplayName(BnplIssuer::IssuerId issuer_id);

// Converts a Bnpl constant into its corresponding enum value.
BnplIssuer::IssuerId ConvertToBnplIssuerIdEnum(std::string_view issuer_id);

// Converts a BNPL enum value into its corresponding constant.
std::string_view ConvertToBnplIssuerIdString(BnplIssuer::IssuerId issuer_id);

// Returns a pair of icon IDs for a BNPL issuer, for light and dark modes
// respectively.
std::pair<BnplIssuer::LightModeImageId, BnplIssuer::DarkModeImageId>
GetBnplIssuerIconIds(BnplIssuer::IssuerId issuer_id, bool issuer_linked);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PAYMENTS_BNPL_ISSUER_H_
