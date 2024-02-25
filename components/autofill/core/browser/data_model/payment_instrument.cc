// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/payment_instrument.h"

namespace autofill {

bool operator==(const PaymentInstrument& a,
                const PaymentInstrument& b) = default;

PaymentInstrument::PaymentInstrument(
    int64_t instrument_id,
    std::u16string_view nickname,
    const GURL& display_icon_url,
    const DenseSet<PaymentInstrument::PaymentRail> supported_rails)
    : instrument_id_(instrument_id),
      nickname_(nickname),
      display_icon_url_(display_icon_url),
      supported_rails_(supported_rails) {}

PaymentInstrument::PaymentInstrument(
    const PaymentInstrument& payment_instrument) = default;
PaymentInstrument& PaymentInstrument::operator=(
    const PaymentInstrument& other) = default;
PaymentInstrument::~PaymentInstrument() = default;

int PaymentInstrument::Compare(const PaymentInstrument& other) const {
  if (instrument_id_ < other.instrument_id()) {
    return -1;
  }

  if (instrument_id_ > other.instrument_id()) {
    return 1;
  }

  int comparison = nickname_.compare(other.nickname());
  if (comparison < 0) {
    return -1;
  } else if (comparison > 0) {
    return 1;
  }

  comparison =
      display_icon_url_.spec().compare(other.display_icon_url().spec());
  if (comparison < 0) {
    return -1;
  } else if (comparison > 0) {
    return 1;
  }

  // Find the first PaymentRail that is different between `this` and the `other`
  // object.
  int index = 0;
  auto this_supported_rail_iter = supported_rails_.begin();
  auto other_supported_rail_iter = other.supported_rails().begin();
  for (; index < (int)supported_rails_.size() &&
         index < (int)other.supported_rails().size();
       index++) {
    int diff = static_cast<int>(*this_supported_rail_iter) -
               static_cast<int>(*other_supported_rail_iter);
    if (diff < 0) {
      return -1;
    } else if (diff > 0) {
      return 1;
    }
    this_supported_rail_iter++;
    other_supported_rail_iter++;
  }
  if (index < (int)supported_rails_.size()) {
    // `this` object has greater supported rails than the `other` object.
    return 1;
  }
  if (index < (int)other.supported_rails().size()) {
    // `other` object has greater supported rails than `this` object.
    return -1;
  }
  return 0;
}

bool PaymentInstrument::IsSupported(PaymentRail payment_rail) const {
  return supported_rails_.contains(payment_rail);
}

}  // namespace autofill
