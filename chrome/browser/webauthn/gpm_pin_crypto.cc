// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm_pin_crypto.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/aead.h"
#include "crypto/hash.h"
#include "crypto/kdf.h"
#include "crypto/random.h"
#include "device/fido/enclave/types.h"

namespace webauthn {

crypto::SubtlePassKey MakeCryptoPassKey() {
  return crypto::SubtlePassKey();
}

// static
PinMetadata PinMetadata::FromProto(
    const webauthn_pb::EnclaveLocalState_WrappedPIN& pin) {
  CHECK_EQ(pin.hash_salt().size(), 16u);
  PinMetadata metadata;
  metadata.n = pin.hash_difficulty();
  metadata.is_six_digits =
      pin.form() == webauthn_pb::EnclaveLocalState_WrappedPIN::FORM_SIX_DIGITS;
  std::ranges::copy(base::as_byte_span(pin.hash_salt()), metadata.salt.begin());
  return metadata;
}

HashedPIN::HashedPIN() = default;

HashedPIN::~HashedPIN() {
  std::ranges::fill(hashed, 0);
}

std::unique_ptr<webauthn_pb::EnclaveLocalState_WrappedPIN>
HashedPIN::ToWrappedPIN() const {
  std::array<uint8_t, 32> claim_key;
  crypto::RandBytes(claim_key);

  auto ret = std::make_unique<webauthn_pb::EnclaveLocalState_WrappedPIN>();
  ret->set_claim_key(std::string(base::as_string_view(claim_key)));
  ret->set_form(
      metadata.is_six_digits
          ? webauthn_pb::EnclaveLocalState_WrappedPIN::FORM_SIX_DIGITS
          : webauthn_pb::EnclaveLocalState_WrappedPIN::FORM_ARBITRARY);
  ret->set_hash(webauthn_pb::EnclaveLocalState_WrappedPIN::HASH_SCRYPT);
  ret->set_hash_difficulty(metadata.n);
  ret->set_hash_salt(std::string(base::as_string_view(metadata.salt)));
  return ret;
}

std::optional<int> CheckPINInvariants(
    const webauthn_pb::EnclaveLocalState_WrappedPIN& wrapped_pin) {
  // The nonce is 12 bytes, and the tag is 16 bytes, so this establishes
  // a lower-bound of one byte of plaintext.
  if (wrapped_pin.wrapped_pin().size() < 12 + 1 + 16) {
    return __LINE__;
  }
  if (wrapped_pin.claim_key().size() != 32) {
    return __LINE__;
  }
  if (wrapped_pin.form() == wrapped_pin.FORM_UNSPECIFIED) {
    return __LINE__;
  }
  if (wrapped_pin.hash() == wrapped_pin.HASH_UNSPECIFIED) {
    return __LINE__;
  }
  if (wrapped_pin.hash_difficulty() <= 0) {
    return __LINE__;
  }
  if (wrapped_pin.hash_salt().empty()) {
    return __LINE__;
  }

  return std::nullopt;
}

std::unique_ptr<HashedPIN> HashPINSlowly(std::string_view pin) {
  auto hashed = std::make_unique<HashedPIN>();
  crypto::RandBytes(hashed->metadata.salt);
  // This is the primary work factor in scrypt. This value matches
  // the original recommended parameters. Those are a little out
  // of date in 2024, but Android is using 4096. Since this work
  // factor falls on the server when MagicArch is used, I've stuck
  // with this norm.
  hashed->metadata.n = 16384;
  hashed->metadata.is_six_digits =
      pin.size() == 6 && std::ranges::all_of(pin, [](char c) -> bool {
        return c >= '0' && c <= '9';
      });
  crypto::kdf::Scrypt({.cost = static_cast<uint64_t>(hashed->metadata.n),
                       .block_size = 8,
                       .parallelization = 1},
                      base::as_byte_span(pin), hashed->metadata.salt,
                      hashed->hashed, MakeCryptoPassKey());
  return hashed;
}

std::unique_ptr<device::enclave::ClaimedPIN> MakeClaimedPINSlowly(
    std::string_view pin,
    const webauthn_pb::EnclaveLocalState_WrappedPIN& wrapped_pin) {
  std::array<uint8_t, 32> hashed;
  crypto::kdf::Scrypt(
      {.cost = static_cast<uint64_t>(wrapped_pin.hash_difficulty()),
       .block_size = 8,
       .parallelization = 1,
       .max_memory_bytes = 1ul << 28},
      base::as_byte_span(pin), base::as_byte_span(wrapped_pin.hash_salt()),
      hashed, MakeCryptoPassKey());

  static constexpr uint8_t kAAD[] = {'P', 'I', 'N', ' ', 'c',
                                     'l', 'a', 'i', 'm'};
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM,
                    base::as_byte_span(wrapped_pin.claim_key()));
  std::array<uint8_t, 12> nonce;
  crypto::RandBytes(nonce);
  std::vector<uint8_t> ciphertext = aead.Seal(hashed, nonce, kAAD);
  ciphertext.insert(ciphertext.begin(), nonce.begin(), nonce.end());

  const auto wrapped_pin_span = base::as_byte_span(wrapped_pin.wrapped_pin());
  return std::make_unique<device::enclave::ClaimedPIN>(
      std::move(ciphertext),
      std::vector<uint8_t>(wrapped_pin_span.begin(), wrapped_pin_span.end()));
}

std::vector<uint8_t> EncryptWrappedPIN(
    base::span<const uint8_t> security_domain_secret,
    base::span<const uint8_t> cbor_bytes) {
  // This is "KeychainApplicationKey:chrome:GPM PIN data wrapping key".
  static constexpr uint8_t kKeyPurposePinDataKey[] = {
      0x4b, 0x65, 0x79, 0x63, 0x68, 0x61, 0x69, 0x6e, 0x41, 0x70, 0x70,
      0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x4b, 0x65, 0x79,
      0x3a, 0x63, 0x68, 0x72, 0x6f, 0x6d, 0x65, 0x3a, 0x47, 0x50, 0x4d,
      0x20, 0x50, 0x49, 0x4e, 0x20, 0x64, 0x61, 0x74, 0x61, 0x20, 0x77,
      0x72, 0x61, 0x70, 0x70, 0x69, 0x6e, 0x67, 0x20, 0x6b, 0x65, 0x79};
  const std::array<uint8_t, 32> derived_key = crypto::kdf::Hkdf<32>(
      crypto::hash::kSha256, security_domain_secret,
      /*salt=*/base::span<const uint8_t>(), kKeyPurposePinDataKey);
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM, derived_key);
  std::array<uint8_t, 12> nonce;
  crypto::RandBytes(nonce);
  std::vector<uint8_t> wrapped_pin = aead.Seal(
      cbor_bytes, nonce, /*additional_data=*/base::span<const uint8_t>());
  wrapped_pin.insert(wrapped_pin.begin(), nonce.begin(), nonce.end());
  return wrapped_pin;
}

std::string MakeWrappedPINForTesting(
    base::span<const uint8_t> security_domain_secret,
    std::string_view pin) {
  std::unique_ptr<HashedPIN> hashed = HashPINSlowly(pin);
  std::unique_ptr<webauthn_pb::EnclaveLocalState_WrappedPIN> wrapped_pin =
      hashed->ToWrappedPIN();
  const std::array<uint8_t, 8> kFakeCounterId = {};
  const std::array<uint8_t, 16> kFakeVaultHandle = {};
  const std::array<uint8_t, 16> kFakeCohortPublicKey = {};
  const int32_t kFakeSerialNumber = 1;

  CHECK_EQ(wrapped_pin->claim_key().size(), 32u);
  cbor::Value::MapValue map;
  map.emplace(1, base::span<const uint8_t>(hashed->hashed));
  // 2 used to correspond to the generation.
  map.emplace(3, base::span<const uint8_t, 32>(
                     base::as_byte_span(wrapped_pin->claim_key())));
  map.emplace(4, base::span<const uint8_t>(kFakeCounterId));
  map.emplace(5, base::span<const uint8_t>(kFakeVaultHandle));
  map.emplace(6, base::span<const uint8_t>(kFakeCohortPublicKey));
  map.emplace(7, kFakeSerialNumber);
  const std::vector<uint8_t> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(map))).value();
  const std::vector<uint8_t> encrypted =
      EncryptWrappedPIN(security_domain_secret, cbor_bytes);
  wrapped_pin->set_wrapped_pin(std::string(base::as_string_view(encrypted)));
  return wrapped_pin->SerializeAsString();
}

}  // namespace webauthn
