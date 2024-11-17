// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/bnpl_issuer.h"

#include <optional>
#include <string>

#include "components/autofill/core/browser/data_model/payment_instrument.h"

namespace autofill {

std::strong_ordering operator<=>(const BnplIssuer&,
                                 const BnplIssuer&) = default;

bool operator==(const BnplIssuer& a, const BnplIssuer& b) = default;

BnplIssuer::BnplIssuer(std::string issuer_id,
                       std::optional<PaymentInstrument> payment_instrument,
                       int price_lower_bound,
                       int price_upper_bound)
    : issuer_id_(std::move(issuer_id)),
      payment_instrument_(payment_instrument),
      price_lower_bound_(price_lower_bound),
      price_upper_bound_(price_upper_bound) {}

BnplIssuer::BnplIssuer(const BnplIssuer&) = default;

BnplIssuer& BnplIssuer::operator=(const BnplIssuer&) = default;

BnplIssuer::BnplIssuer(BnplIssuer&&) = default;

BnplIssuer& BnplIssuer::operator=(BnplIssuer&&) = default;

BnplIssuer::~BnplIssuer() = default;

}  // namespace autofill
