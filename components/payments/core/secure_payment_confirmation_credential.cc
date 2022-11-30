// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/secure_payment_confirmation_credential.h"

#include <utility>

#include "base/metrics/histogram_functions.h"

namespace payments {

SecurePaymentConfirmationCredential::SecurePaymentConfirmationCredential() =
    default;

SecurePaymentConfirmationCredential::SecurePaymentConfirmationCredential(
    std::vector<uint8_t> credential_id,
    const std::string& relying_party_id,
    std::vector<uint8_t> user_id)
    : credential_id(std::move(credential_id)),
      relying_party_id(relying_party_id),
      user_id(std::move(user_id)) {
  // Record the size of credential_id to see whether or not hashing is needed
  // before storing in DB. crbug.com/1122764
  base::UmaHistogramCounts10000(
      "PaymentRequest.SecurePaymentConfirmationCredentialIdSizeInBytes",
      this->credential_id.size());
}

SecurePaymentConfirmationCredential::~SecurePaymentConfirmationCredential() =
    default;

bool SecurePaymentConfirmationCredential::IsValid() const {
  return !credential_id.empty() && !relying_party_id.empty();
}

bool SecurePaymentConfirmationCredential::IsValidNewCredential() const {
  return IsValid() && !user_id.empty();
}

}  // namespace payments
