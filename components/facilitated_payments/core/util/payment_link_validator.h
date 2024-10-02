// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_UTIL_PAYMENT_LINK_VALIDATOR_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_UTIL_PAYMENT_LINK_VALIDATOR_H_

#include <string>
#include <vector>

class PaymentLinkValidator {
 public:
  PaymentLinkValidator();
  ~PaymentLinkValidator();

  PaymentLinkValidator(const PaymentLinkValidator&) = delete;
  PaymentLinkValidator& operator=(const PaymentLinkValidator&) = delete;

  bool IsValid(std::string_view url) const;

 private:
  const std::vector<std::string> valid_prefixes_;
};

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_UTIL_PAYMENT_LINK_VALIDATOR_H_
