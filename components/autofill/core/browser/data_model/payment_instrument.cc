// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/payment_instrument.h"

namespace autofill {

PaymentInstrument::PaymentInstrument(int64_t instrument_id,
                                     Nickname nickname,
                                     const GURL& display_icon_url)
    : instrument_id_(instrument_id),
      nickname_(nickname),
      display_icon_url_(display_icon_url) {}

PaymentInstrument::PaymentInstrument(
    const PaymentInstrument& payment_instrument) = default;

PaymentInstrument::~PaymentInstrument() = default;

void PaymentInstrument::AddPaymentRail(PaymentRail payment_rail) {
  supported_rails_.insert(payment_rail);
}

bool PaymentInstrument::IsSupported(PaymentRail payment_rail) const {
  return supported_rails_.contains(payment_rail);
}

}  // namespace autofill
