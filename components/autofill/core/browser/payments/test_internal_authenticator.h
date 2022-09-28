// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_INTERNAL_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_INTERNAL_AUTHENTICATOR_H_

#include "components/webauthn/core/browser/internal_authenticator.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

namespace autofill {

// Subclass of InternalAuthenticator meant for testing purposes.
class TestInternalAuthenticator : public webauthn::InternalAuthenticator {
 public:
  TestInternalAuthenticator() = default;
  ~TestInternalAuthenticator() override = default;

  // InternalAuthenticator:
  void SetEffectiveOrigin(const url::Origin& origin) override {}
  void SetPaymentOptions(blink::mojom::PaymentOptionsPtr payment) override {}
  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      blink::mojom::Authenticator::MakeCredentialCallback callback) override {}
  void GetAssertion(
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
      blink::mojom::Authenticator::GetAssertionCallback callback) override {}
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback)
      override;
  bool IsGetMatchingCredentialIdsSupported() override;
  void GetMatchingCredentialIds(
      const std::string& relying_party_id,
      const std::vector<std::vector<uint8_t>>& credential_ids,
      bool require_third_party_payment_bit,
      webauthn::GetMatchingCredentialIdsCallback callback) override {}
  void Cancel() override {}
  content::RenderFrameHost* GetRenderFrameHost() override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_INTERNAL_AUTHENTICATOR_H_
