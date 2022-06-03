// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/fake_auth_provider.h"

namespace chromeos {
namespace libassistant {

std::string FakeAuthProvider::GetAuthClientId() {
  return "kFakeClientId";
}

std::vector<std::string> FakeAuthProvider::GetClientCertificateChain() {
  return {};
}

void FakeAuthProvider::CreateCredentialAttestationJwt(
    const std::string& authorization_code,
    const std::vector<std::pair<std::string, std::string>>& claims,
    CredentialCallback attestation_callback) {
  attestation_callback(Error::SUCCESS, "", "");
}

void FakeAuthProvider::CreateRefreshAssertionJwt(
    const std::string& key_identifier,
    const std::vector<std::pair<std::string, std::string>>& claims,
    AssertionCallback assertion_callback) {
  assertion_callback(Error::SUCCESS, "");
}

void FakeAuthProvider::CreateDeviceAttestationJwt(
    const std::vector<std::pair<std::string, std::string>>& claims,
    AssertionCallback attestation_callback) {
  attestation_callback(Error::SUCCESS, "");
}

std::string FakeAuthProvider::GetAttestationCertFingerprint() {
  return "kFakeAttestationCertFingerprint";
}

void FakeAuthProvider::RemoveCredentialKey(const std::string& key_identifier) {}

void FakeAuthProvider::Reset() {}

}  // namespace libassistant
}  // namespace chromeos
