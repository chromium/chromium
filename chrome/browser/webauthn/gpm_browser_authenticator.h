// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_BROWSER_AUTHENTICATOR_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_BROWSER_AUTHENTICATOR_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "device/fido/fido_authenticator.h"

namespace device {
struct CtapGetAssertionOptions;
struct CtapGetAssertionRequest;
struct CtapMakeCredentialRequest;
struct MakeCredentialOptions;
}  // namespace device

// Parameters and callbacks passed from the controller to
// `GpmBrowserAuthenticator` for a `MakeCredential` or `GetAssertion` operation.
struct GpmBrowserAuthenticatorRequest {
  using SavePasskeyCallback =
      base::RepeatingCallback<void(sync_pb::WebauthnCredentialSpecifics)>;

  struct MakeCredential {
    explicit MakeCredential(
        trusted_vault::TrustedVaultKeyAndVersion trusted_vault_key);
    MakeCredential(MakeCredential&&);
    MakeCredential& operator=(MakeCredential&&);
    MakeCredential(const MakeCredential&) = delete;
    MakeCredential& operator=(const MakeCredential&) = delete;
    ~MakeCredential();

    trusted_vault::TrustedVaultKeyAndVersion trusted_vault_key;
    // Existing credential IDs for the RP, checked against `exclude_list`.
    std::vector<std::vector<uint8_t>> existing_cred_ids;
  };

  struct GetAssertion {
    GetAssertion(std::vector<std::vector<uint8_t>> trusted_vault_keys,
                 sync_pb::WebauthnCredentialSpecifics entity);
    GetAssertion(GetAssertion&&);
    GetAssertion& operator=(GetAssertion&&);
    GetAssertion(const GetAssertion&) = delete;
    GetAssertion& operator=(const GetAssertion&) = delete;
    ~GetAssertion();

    // Ordered from oldest to newest.
    std::vector<std::vector<uint8_t>> trusted_vault_keys;
    // The credential selected for `GetAssertion`.
    sync_pb::WebauthnCredentialSpecifics entity;
  };

  explicit GpmBrowserAuthenticatorRequest(
      std::variant<MakeCredential, GetAssertion> data);
  GpmBrowserAuthenticatorRequest(GpmBrowserAuthenticatorRequest&&);
  GpmBrowserAuthenticatorRequest& operator=(GpmBrowserAuthenticatorRequest&&);
  GpmBrowserAuthenticatorRequest(const GpmBrowserAuthenticatorRequest&) =
      delete;
  GpmBrowserAuthenticatorRequest& operator=(
      const GpmBrowserAuthenticatorRequest&) = delete;
  ~GpmBrowserAuthenticatorRequest();

  std::variant<MakeCredential, GetAssertion> data;
  // Value for the UV flag in the authenticator data.
  bool user_verified = false;
  // Persists the newly created or updated passkey.
  SavePasskeyCallback save_passkey_callback;
};

// GpmBrowserAuthenticator creates and asserts GPM passkeys locally in the
// browser.
class GpmBrowserAuthenticator : public device::FidoAuthenticator {
 public:
  explicit GpmBrowserAuthenticator(GpmBrowserAuthenticatorRequest request);
  ~GpmBrowserAuthenticator() override;

  GpmBrowserAuthenticator(const GpmBrowserAuthenticator&) = delete;
  GpmBrowserAuthenticator& operator=(const GpmBrowserAuthenticator&) = delete;

  // device::FidoAuthenticator:
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void MakeCredential(device::CtapMakeCredentialRequest request,
                      device::MakeCredentialOptions options,
                      MakeCredentialCallback callback) override;
  void GetAssertion(device::CtapGetAssertionRequest request,
                    device::CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;
  std::optional<base::span<const int32_t>> GetAlgorithms() override;
  void Cancel() override;
  device::AuthenticatorType GetType() const override;
  std::string GetId() const override;
  const device::AuthenticatorSupportedOptions& Options() const override;
  std::optional<device::FidoTransportProtocol> AuthenticatorTransport()
      const override;
  base::WeakPtr<device::FidoAuthenticator> GetWeakPtr() override;

 private:
  const std::array<uint8_t, 8> id_;
  GpmBrowserAuthenticatorRequest request_;

  base::WeakPtrFactory<GpmBrowserAuthenticator> weak_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_BROWSER_AUTHENTICATOR_H_
