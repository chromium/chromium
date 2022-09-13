// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENTS_VALIDATORS_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENTS_VALIDATORS_H_

#include <string>

#include "components/payments/mojom/payment_request_data.mojom.h"

namespace payments {

class PaymentsValidators {
 public:
  PaymentsValidators() = delete;
  PaymentsValidators(const PaymentsValidators&) = delete;
  PaymentsValidators& operator=(const PaymentsValidators&) = delete;

  // The most common identifiers are three-letter alphabetic codes as
  // defined by [ISO4217] (for example, "USD" for US Dollars).
  static bool IsValidCurrencyCodeFormat(const std::string& code,
                                        std::string* optional_error_message);

  // Returns true if |amount| is a valid currency code as defined in ISO 20022
  // CurrencyAnd30Amount.
  static bool IsValidAmountFormat(const std::string& amount,
                                  std::string* optional_error_message);

  // Returns true if |code| is a valid ISO 3166 country code.
  static bool IsValidCountryCodeFormat(const std::string& code,
                                       std::string* optional_error_message);

  // Returns false if |error| is too long (greater than 2048).
  static bool IsValidErrorMsgFormat(const std::string& code,
                                    std::string* optional_error_message);

  // Returns false and optionally populate |optional_error_message| if any
  // fields of |errors| have too long string (greater than 2048).
  static bool IsValidAddressErrorsFormat(const mojom::AddressErrorsPtr& errors,
                                         std::string* optional_error_message);

  // Returns false and optionally populate |optional_error_message| if any
  // fields of |errors| have too long string (greater than 2048).
  static bool IsValidPayerErrorsFormat(const mojom::PayerErrorsPtr& errors,
                                       std::string* optional_error_message);

  // Returns false and optionally populate |optional_error_message| if any
  // fields of |errors| have too long string (greater than 2048).
  static bool IsValidPaymentValidationErrorsFormat(
      const mojom::PaymentValidationErrorsPtr& errors,
      std::string* optional_error_message);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENTS_VALIDATORS_H_
