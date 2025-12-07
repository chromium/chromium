// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/validation/payment_link_validator.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace payments::facilitated {

// NOTE: The valid prefixes list may change over time. This list is
// expected to be finalized and aligned with the requirements of the
// eWallet push payment project and A2A payment project.
// When the list is being updated, please also update the payment link
// spec at https://github.com/WICG/paymentlink/blob/main/index.bs, and
// the public design at https://bit.ly/html-payment-link-dd.
static constexpr std::array kValidPrefixes = std::to_array<std::string_view>(
    {"duitnow://paynet.com.my", "shopeepay://shopeepay.com.my",
     "tngd://tngdigital.com.my",
     "https://www.itmx.co.th/facilitated-payment/prompt-pay", "momo://app?",
     "https://api.doku.com/facilitated-payment/dana",
     "https://dana.id/facilitated-payment/dana"});

PaymentLinkValidator::PaymentLinkValidator() = default;

PaymentLinkValidator::~PaymentLinkValidator() = default;

PaymentLinkValidator::Scheme PaymentLinkValidator::GetScheme(
    const GURL& payment_link_url) const {
  if (!payment_link_url.is_valid()) {
    return Scheme::kInvalid;
  }

  std::string_view spec = payment_link_url.spec();
  if (std::ranges::none_of(kValidPrefixes, [&spec](std::string_view prefix) {
        return spec.starts_with(prefix);
      })) {
    return Scheme::kInvalid;
  }

  if (payment_link_url.SchemeIs("duitnow")) {
    return Scheme::kDuitNow;
  }
  if (payment_link_url.SchemeIs("shopeepay")) {
    return Scheme::kShopeePay;
  }
  if (payment_link_url.SchemeIs("tngd")) {
    return Scheme::kTngd;
  }
  if (payment_link_url.SchemeIs("momo")) {
    return Scheme::kMomo;
  }
  if (payment_link_url.path() == "/facilitated-payment/prompt-pay" &&
      spec.starts_with(
          "https://www.itmx.co.th/facilitated-payment/prompt-pay")) {
    return Scheme::kPromptPay;
  }
  if (payment_link_url.path() == "/facilitated-payment/dana" &&
      (spec.starts_with("https://dana.id/facilitated-payment/dana") ||
       spec.starts_with("https://api.doku.com/facilitated-payment/dana"))) {
    return Scheme::kDana;
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
