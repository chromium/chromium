// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_AUTHENTICATOR_H_
#define CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_AUTHENTICATOR_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/global_routing_id.h"
#include "device/fido/fido_authenticator.h"

namespace content {
class RenderFrameHost;
}

namespace webauthn {
class PasskeyModel;
}

namespace chromeos {

class PasskeyService;

class PasskeyAuthenticator : public device::FidoAuthenticator {
 public:
  // `rfh`, `passkey_service` and `rfh` must be non-null and outlive the
  // PasskeyAuthenticator.
  PasskeyAuthenticator(content::RenderFrameHost* rfh,
                       PasskeyService* passkey_service,
                       webauthn::PasskeyModel* model);
  ~PasskeyAuthenticator() override;

  // device::FidoAuthenticator:
  void InitializeAuthenticator(base::OnceClosure callback) override;
  std::optional<base::span<const int32_t>> GetAlgorithms() override;
  void MakeCredential(device::CtapMakeCredentialRequest request,
                      device::MakeCredentialOptions request_options,
                      MakeCredentialCallback callback) override;
  void GetAssertion(device::CtapGetAssertionRequest request,
                    device::CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;
  void Cancel() override;
  device::AuthenticatorType GetType() const override;
  std::string GetId() const override;
  const device::AuthenticatorSupportedOptions& Options() const override;
  std::optional<device::FidoTransportProtocol> AuthenticatorTransport()
      const override;
  void GetTouch(base::OnceClosure callback) override;

  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

 private:
  const content::GlobalRenderFrameHostId render_frame_host_id_;
  const raw_ptr<PasskeyService> passkey_service_;
  const raw_ptr<webauthn::PasskeyModel> passkey_model_;

  void FinishGetAssertion(device::CtapGetAssertionRequest request,
                          device::CtapGetAssertionOptions options,
                          GetAssertionCallback callback,
                          bool user_verification_success);

  base::WeakPtrFactory<PasskeyAuthenticator> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_AUTHENTICATOR_H_
