// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/secure_payment_confirmation_instrument.h"

#include <utility>

#include "base/metrics/histogram_functions.h"

namespace payments {

SecurePaymentConfirmationInstrument::SecurePaymentConfirmationInstrument() =
    default;

SecurePaymentConfirmationInstrument::SecurePaymentConfirmationInstrument(
    std::vector<uint8_t> credential_id,
    const std::string& relying_party_id)
    : credential_id(std::move(credential_id)),
      relying_party_id(relying_party_id) {
  // Record the size of credential_id to see whether or not hashing is needed
  // before storing in DB. crbug.com/1122764
  base::UmaHistogramCounts10000(
      "PaymentRequest.SecurePaymentConfirmationCredentialIdSizeInBytes",
      this->credential_id.size());
}

SecurePaymentConfirmationInstrument::~SecurePaymentConfirmationInstrument() =
    default;

bool SecurePaymentConfirmationInstrument::IsValid() const {
  return !credential_id.empty() && !relying_party_id.empty();
}

}  // namespace payments
