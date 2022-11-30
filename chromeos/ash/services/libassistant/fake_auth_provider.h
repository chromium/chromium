// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_FAKE_AUTH_PROVIDER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_FAKE_AUTH_PROVIDER_H_

#include "chromeos/assistant/internal/libassistant/shared_headers.h"

#include <string>
#include <vector>

namespace ash::libassistant {

// ChromeOS does not use auth manager, so we don't yet need to implement a
// real auth provider.
class FakeAuthProvider : public assistant_client::AuthProvider {
 public:
  FakeAuthProvider() = default;
  ~FakeAuthProvider() override = default;

  // assistant_client::AuthProvider implementation:
  std::string GetAuthClientId() override;
  std::vector<std::string> GetClientCertificateChain() override;

  void CreateCredentialAttestationJwt(
      const std::string& authorization_code,
      const std::vector<std::pair<std::string, std::string>>& claims,
      CredentialCallback attestation_callback) override;

  void CreateRefreshAssertionJwt(
      const std::string& key_identifier,
      const std::vector<std::pair<std::string, std::string>>& claims,
      AssertionCallback assertion_callback) override;

  void CreateDeviceAttestationJwt(
      const std::vector<std::pair<std::string, std::string>>& claims,
      AssertionCallback attestation_callback) override;

  std::string GetAttestationCertFingerprint() override;

  void RemoveCredentialKey(const std::string& key_identifier) override;

  void Reset() override;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_FAKE_AUTH_PROVIDER_H_
