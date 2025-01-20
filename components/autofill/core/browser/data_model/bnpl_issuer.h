// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BNPL_ISSUER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BNPL_ISSUER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/payment_instrument.h"

namespace autofill {

// Contains information regarding a Buy Now Pay Later issuer that the user is
// eligible to use on certain merchant webpages.
class BnplIssuer {
 public:
  // Struct that links currency to the eligible price range in that currency for
  // a BNPL issuer.
  struct EligiblePriceRange {
    EligiblePriceRange(std::string currency,
                       uint64_t price_lower_bound,
                       uint64_t price_upper_bound)
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
    uint64_t price_lower_bound;

    // Upper bound for the US dollar price range that this BNPL provider
    // accepts, in micros of currency. A micro of currency is one millionths of
    // the base unit (dollars, not cents for example). e.g. $1.05 == 1050000.
    // Bound is inclusive.
    uint64_t price_upper_bound;
  };

  // `instrument_id` is present for linked issuers, and nullopt for unlinked
  // issuers. `issuer_id` is the unique identifier of this specfiic issuer.
  // `eligible_price_ranges` is a list of currencies mapped to their price
  // ranges, in micros.
  BnplIssuer(std::optional<int64_t> instrument_id,
             std::string issuer_id,
             std::vector<EligiblePriceRange> eligible_price_ranges);
  BnplIssuer(const BnplIssuer&);
  BnplIssuer& operator=(const BnplIssuer&);
  BnplIssuer(BnplIssuer&&);
  BnplIssuer& operator=(BnplIssuer&&);
  ~BnplIssuer();
  friend bool operator==(const BnplIssuer&, const BnplIssuer&);

  const std::string& issuer_id() const { return issuer_id_; }
  void set_issuer_id(std::string issuer_id) {
    issuer_id_ = std::move(issuer_id);
  }

  const std::optional<PaymentInstrument>& payment_instrument() const {
    return payment_instrument_;
  }
  void set_payment_instrument(
      std::optional<PaymentInstrument> payment_instrument) {
    payment_instrument_ = payment_instrument;
  }

  base::span<const EligiblePriceRange> eligible_price_ranges() const {
    return eligible_price_ranges_;
  }
  void set_eligible_price_ranges(
      std::vector<EligiblePriceRange> eligible_price_ranges) {
    eligible_price_ranges_ = std::move(eligible_price_ranges);
  }

 private:
  // Unique identifier for the BNPL partner.
  std::string issuer_id_;

  // If the issuer is linked, `payment_instrument_` will contain the
  // instrument_id. If the issuer is unlinked, `payment_instrument_` will be
  // empty.
  std::optional<PaymentInstrument> payment_instrument_;

  // Vector of eligible price ranges for this BnplIssuer. Contains per-currency
  // eligible price ranges for BNPL, for all supported currencies.
  std::vector<EligiblePriceRange> eligible_price_ranges_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BNPL_ISSUER_H_
