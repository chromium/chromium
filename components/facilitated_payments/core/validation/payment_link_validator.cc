// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/validation/payment_link_validator.h"

#include <algorithm>

namespace payments::facilitated {

PaymentLinkValidator::PaymentLinkValidator()
    : valid_prefixes_{
          // NOTE: The valid prefixes list may change over time. This list is
          // expected to be finalized and aligned with the requirements of the
          // eWallet push payment project.
          // dd: bit.ly/html-payment-link-dd (Payment Link Examples section and
          // Security section)
          "duitnow://paynet.com.my", "shopeepay://shopeepay.com.my",
          "tngd://tngdigital.com.my"} {}

PaymentLinkValidator::~PaymentLinkValidator() = default;

PaymentLinkValidator::Scheme PaymentLinkValidator::GetScheme(
    const GURL& payment_link_url) const {
  if (!payment_link_url.is_valid()) {
    return Scheme::kInvalid;
  }
  if (!std::any_of(valid_prefixes_.begin(), valid_prefixes_.end(),
                   [&payment_link_url](const std::string& prefix) {
                     return payment_link_url.spec().find(prefix) == 0;
                   })) {
    return Scheme::kInvalid;
  }

  if (payment_link_url.scheme() == "duitnow") {
    return Scheme::kDuitNow;
  }
  if (payment_link_url.scheme() == "shopeepay") {
    return Scheme::kShopeePay;
  }
  if (payment_link_url.scheme() == "tngd") {
    return Scheme::kTngd;
  }
  return Scheme::kInvalid;
}

}  // namespace payments::facilitated
