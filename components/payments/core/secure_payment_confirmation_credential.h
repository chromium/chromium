// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_H_
#define COMPONENTS_PAYMENTS_CORE_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_H_

#include <stdint.h>

#include <string>
#include <vector>

namespace payments {

// Secure Payment Confirmation credential information that can be stored in an
// SQLite database.
struct SecurePaymentConfirmationCredential {
  // Constructs an empty credential. This credential is not valid until all
  // fields are populated.
  SecurePaymentConfirmationCredential();

  // Constructs a credential with the given fields. Please use `std::move()`
  // when passing the `credential_id` and `user_id` byte arrays to avoid
  // excessive copying.
  SecurePaymentConfirmationCredential(std::vector<uint8_t> credential_id,
                                      const std::string& relying_party_id,
                                      std::vector<uint8_t> user_id);

  ~SecurePaymentConfirmationCredential();

  // A credential should not be copied or assigned.
  SecurePaymentConfirmationCredential(
      const SecurePaymentConfirmationCredential& other) = delete;
  SecurePaymentConfirmationCredential& operator=(
      const SecurePaymentConfirmationCredential& other) = delete;

  // Checks credential validity for an existing credential, which may not have
  // a user ID.
  bool IsValid() const;

  // Checks credential validity for a newly created credential, which requires a
  // user ID.
  bool IsValidNewCredential() const;

  std::vector<uint8_t> credential_id;
  std::string relying_party_id;
  std::vector<uint8_t> user_id;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_H_
