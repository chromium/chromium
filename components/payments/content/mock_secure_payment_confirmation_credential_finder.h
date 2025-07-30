// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_MOCK_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_FINDER_H_
#define COMPONENTS_PAYMENTS_CONTENT_MOCK_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_FINDER_H_

#include "components/payments/content/secure_payment_confirmation_credential_finder.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace payments {

class MockSecurePaymentConfirmationCredentialFinder
    : public SecurePaymentConfirmationCredentialFinder {
 public:
  MockSecurePaymentConfirmationCredentialFinder();
  ~MockSecurePaymentConfirmationCredentialFinder() override;

  // SecurePaymentConfirmationCredentialFinder:
  MOCK_METHOD(
      void,
      GetMatchingCredentials,
      (const std::vector<std::vector<uint8_t>>& credential_ids,
       const std::string& relying_party_id,
       const url::Origin& caller_origin,
       webauthn::InternalAuthenticator* authenticator,
       scoped_refptr<payments::WebPaymentsWebDataService> web_data_service,
       SecurePaymentConfirmationCredentialFinderCallback result_callback),
      (override));
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_MOCK_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_FINDER_H_
