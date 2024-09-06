// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/payment_instrument.h"

namespace autofill {

std::strong_ordering operator<=>(const PaymentInstrument& a,
                                 const PaymentInstrument& b) = default;

bool operator==(const PaymentInstrument& a,
                const PaymentInstrument& b) = default;

PaymentInstrument::PaymentInstrument(
    int64_t instrument_id,
    std::u16string nickname,
    GURL display_icon_url,
    DenseSet<PaymentInstrument::PaymentRail> supported_rails,
    bool is_fido_enrolled)
    : instrument_id_(instrument_id),
      nickname_(std::move(nickname)),
      display_icon_url_(std::move(display_icon_url)),
      supported_rails_(supported_rails),
      is_fido_enrolled_(is_fido_enrolled) {}

PaymentInstrument::PaymentInstrument(
    const PaymentInstrument& payment_instrument) = default;
PaymentInstrument& PaymentInstrument::operator=(
    const PaymentInstrument& other) = default;
PaymentInstrument::~PaymentInstrument() = default;

bool PaymentInstrument::IsSupported(PaymentRail payment_rail) const {
  return supported_rails_.contains(payment_rail);
}

}  // namespace autofill
