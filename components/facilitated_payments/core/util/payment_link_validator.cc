// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/util/payment_link_validator.h"

#include <algorithm>

PaymentLinkValidator::PaymentLinkValidator()
    : valid_prefixes_{
          // NOTE: The valid prefixes list may change over time. This list is
          // expected to be finalized and aligned with the requirements of the
          // eWallet push payment project.
          // dd: bit.ly/html-payment-link-dd (Payment Link Examples section and
          // Security section)
          "duitnow://shopeepay.com.my", "duitnow://tngdigital.com.my",
          "shopeepay://shopeepay.com.my", "tngditial://tngdigital.com.my"} {}

PaymentLinkValidator::~PaymentLinkValidator() = default;

bool PaymentLinkValidator::IsValid(std::string_view url) const {
  return std::any_of(
      valid_prefixes_.begin(), valid_prefixes_.end(),
      [&url](const std::string& prefix) { return url.find(prefix) == 0; });
}
