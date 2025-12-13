// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_VALIDATION_PAYMENT_LINK_VALIDATOR_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_VALIDATION_PAYMENT_LINK_VALIDATOR_H_

#include "url/gurl.h"

namespace payments::facilitated {

class PaymentLinkValidator {
 public:
  // The list of supported payment link schemes.
  enum class Scheme {
    kInvalid = 0,
    kDuitNow = 1,
    kShopeePay = 2,
    kTngd = 3,
    kPromptPay = 4,
    kMomo = 5,
    kDana = 6,
  };

  PaymentLinkValidator();
  ~PaymentLinkValidator();

  PaymentLinkValidator(const PaymentLinkValidator&) = delete;
  PaymentLinkValidator& operator=(const PaymentLinkValidator&) = delete;

  // Returns the `Scheme` of the given `payment_link_url`.
  Scheme GetScheme(const GURL& payment_link_url) const;

  // Sanitizes a GURL by removing components that are not needed for payment app
  // retrieval.
  static GURL SanitizeForPaymentAppRetrieval(const GURL& payment_link_url);
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_VALIDATION_PAYMENT_LINK_VALIDATOR_H_
