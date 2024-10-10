// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BNPL_ISSUER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BNPL_ISSUER_H_

#include <optional>
#include <string>

#include "components/autofill/core/browser/data_model/payment_instrument.h"

namespace autofill {

// Contains information regarding a Buy Now Pay Later issuer that the user is
// eligible to use on certain merchant webpages.
class BnplIssuer {
 public:
  BnplIssuer(std::string issuer_id,
             std::optional<PaymentInstrument> payment_instrument,
             int price_lower_bound,
             int price_upper_bound);
  BnplIssuer(const BnplIssuer&);
  BnplIssuer& operator=(const BnplIssuer&);
  BnplIssuer(BnplIssuer&&);
  BnplIssuer& operator=(BnplIssuer&&);
  ~BnplIssuer();
  friend std::strong_ordering operator<=>(const BnplIssuer&, const BnplIssuer&);
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

  int price_lower_bound() const { return price_lower_bound_; }
  void set_price_lower_bound(int price_lower_bound) {
    price_lower_bound_ = price_lower_bound;
  }

  int price_upper_bound() const { return price_upper_bound_; }
  void set_price_upper_bound(int price_upper_bound) {
    price_upper_bound_ = price_upper_bound;
  }

 private:
  // Unique identifier for the BNPL partner.
  std::string issuer_id_;

  // If the issuer is linked, `payment_instrument_` will contain the
  // instrument_id. If the issuer is unlinked, `payment_instrument_` will be
  // empty.
  std::optional<PaymentInstrument> payment_instrument_;

  // Lower bound for the US dollar price range that this BNPL provider accepts.
  // Bound is inclusive.
  int price_lower_bound_;

  // Upper bound for the US dollar price range that this BNPL provider accepts.
  // Bound is inclusive.
  int price_upper_bound_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BNPL_ISSUER_H_
