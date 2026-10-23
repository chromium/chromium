// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_PIN_CRYPTO_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_PIN_CRYPTO_H_

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "crypto/subtle_passkey.h"

namespace device::enclave {
struct ClaimedPIN;
}  // namespace device::enclave

namespace webauthn_pb {
class EnclaveLocalState_WrappedPIN;
}  // namespace webauthn_pb

namespace webauthn {

crypto::SubtlePassKey MakeCryptoPassKey();

// Stores public metadata about a PIN. This is recorded in, for example, the
// Vault metadata so that MagicArch can show the correct UI and accept GPM PIN
// entries.
struct PinMetadata {
  static PinMetadata FromProto(
      const webauthn_pb::EnclaveLocalState_WrappedPIN& pin);

  int n = 0;  // The scrypt `N` parameter.
  bool is_six_digits = false;
  std::array<uint8_t, 16> salt = {};
};

struct HashedPIN {
  HashedPIN();
  ~HashedPIN();

  // Copies the values of this structure into a `WrappedPIN` protobuf with a
  // random claim key. The inner `wrapped_pin` member is not set and needs to be
  // filled in by the caller once that value is available.
  std::unique_ptr<webauthn_pb::EnclaveLocalState_WrappedPIN> ToWrappedPIN()
      const;

  PinMetadata metadata;
  std::array<uint8_t, 32> hashed = {};
};

std::optional<int> CheckPINInvariants(
    const webauthn_pb::EnclaveLocalState_WrappedPIN& wrapped_pin);

std::unique_ptr<HashedPIN> HashPINSlowly(std::string_view pin);

// Slowly computes a PIN claim for `pin` and `wrapped_pin` for submission to the
// enclave.
std::unique_ptr<device::enclave::ClaimedPIN> MakeClaimedPINSlowly(
    std::string_view pin,
    const webauthn_pb::EnclaveLocalState_WrappedPIN& wrapped_pin);

// Encrypts `cbor_bytes` representing an inner wrapped PIN CBOR map with a key
// derived from `security_domain_secret`.
std::vector<uint8_t> EncryptWrappedPIN(
    base::span<const uint8_t> security_domain_secret,
    base::span<const uint8_t> cbor_bytes);

// Creates a serialized `EnclaveLocalState::WrappedPIN` protobuf suitable for
// putting into a simulated security domain member in tests.
std::string MakeWrappedPINForTesting(
    base::span<const uint8_t> security_domain_secret,
    std::string_view pin);

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_PIN_CRYPTO_H_
