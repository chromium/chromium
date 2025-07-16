// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/validation/payment_link_validator.h"

#include <algorithm>

#include "base/strings/string_util.h"

namespace payments::facilitated {

PaymentLinkValidator::PaymentLinkValidator()
    : valid_prefixes_{
          // NOTE: The valid prefixes list may change over time. This list is
          // expected to be finalized and aligned with the requirements of the
          // eWallet push payment project and A2A payment project.
          // When the list is being updated, please also update the payment link
          // spec at https://github.com/WICG/paymentlink/blob/main/index.bs, and
          // the public design at https://bit.ly/html-payment-link-dd.
          "duitnow://paynet.com.my", "shopeepay://shopeepay.com.my",
          "tngd://tngdigital.com.my",
          "https://www.itmx.co.th/facilitated-payment/prompt-pay",
          "momo://app?"} {}

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
  if (payment_link_url.scheme() == "momo") {
    return Scheme::kMomo;
  }
  if (payment_link_url.path() == "/facilitated-payment/prompt-pay" &&
      base::StartsWith(payment_link_url.spec(),
                       "https://www.itmx.co.th/facilitated-payment/prompt-pay",
                       base::CompareCase::SENSITIVE)) {
    return Scheme::kPromptPay;
  }
  return Scheme::kInvalid;
}

// static
GURL PaymentLinkValidator::SanitizeForPaymentAppRetrieval(
    const GURL& payment_link_url) {
  GURL::Replacements replacements;

  replacements.ClearQuery();
  replacements.ClearRef();
  replacements.ClearPort();
  replacements.ClearUsername();
  replacements.ClearPassword();

  return payment_link_url.ReplaceComponents(replacements);
}

}  // namespace payments::facilitated
