// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_SECURE_PAYMENT_CONFIRMATION_INSTRUMENT_H_
#define COMPONENTS_PAYMENTS_CORE_SECURE_PAYMENT_CONFIRMATION_INSTRUMENT_H_

#include <stdint.h>

#include <string>
#include <vector>

namespace payments {

// Secure payment information instrument information that can be stored in
// SQLite database.
struct SecurePaymentConfirmationInstrument {
  // Constructs an empty instrument. This instrument is not valid until all
  // fields are populated.
  SecurePaymentConfirmationInstrument();

  // Constructs an instrument with the given fields. Please use `std::move()`
  // when passing the `credential_id` byte array to avoid excessive copying.
  SecurePaymentConfirmationInstrument(std::vector<uint8_t> credential_id,
                                      const std::string& relying_party_id);

  ~SecurePaymentConfirmationInstrument();

  // An instrument should not be copied or assigned.
  SecurePaymentConfirmationInstrument(
      const SecurePaymentConfirmationInstrument& other) = delete;
  SecurePaymentConfirmationInstrument& operator=(
      const SecurePaymentConfirmationInstrument& other) = delete;

  // Checks instrument validity.
  bool IsValid() const;

  std::vector<uint8_t> credential_id;
  std::string relying_party_id;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_SECURE_PAYMENT_CONFIRMATION_INSTRUMENT_H_
