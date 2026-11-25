// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm/local_gpm_pin_crypto.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/webauthn/gpm_pin_crypto.h"
#include "components/trusted_vault/gpm_pin_recovery_factor.h"
#include "crypto/kdf.h"
#include "crypto/process_bound_string.h"
#include "crypto/random.h"
#include "crypto/secure_util.h"

namespace webauthn {

namespace {

constexpr size_t kScryptHashSize = 32;
constexpr size_t kScryptSaltSize = 16;
// Matches the desktop enclave scrypt default (N=16384, r=8, p=1);
// `VerifyGpmPinSlowly()` also accepts other valid powers of two such as
// Android's N=4096.
constexpr int kDefaultScryptDifficulty = 16384;
// Matches `MakeClaimedPINSlowly()`'s 256 MiB memory cap. With r=8 and p=1,
// each scrypt block is 1024 bytes, so 2^17 (131072) is the largest
// power-of-two cost parameter guaranteed to fit within 256 MiB in BoringSSL's
// `EVP_PBE_scrypt()`.
constexpr uint64_t kScryptMaxMemoryBytes = uint64_t{1} << 28;
constexpr int kMaxScryptDifficulty = 1 << 17;

}  // namespace

trusted_vault::GpmPinRecoveryFactor::HashedPin HashGpmPinSlowly(
    const GpmPin& pin) {
  base::AssertLongCPUWorkAllowed();
  CHECK(!pin->empty());
  std::vector<uint8_t> salt = crypto::RandBytesAsVector(kScryptSaltSize);

  crypto::SecureString decrypted_pin = pin->secure_value();
  const bool is_six_digits = decrypted_pin.size() == 6 &&
                             std::ranges::all_of(decrypted_pin, [](char c) {
                               return c >= '0' && c <= '9';
                             });

  std::string hash_bytes(kScryptHashSize, '\0');
  crypto::kdf::Scrypt({.cost = static_cast<uint64_t>(kDefaultScryptDifficulty),
                       .block_size = 8,
                       .parallelization = 1,
                       .max_memory_bytes = kScryptMaxMemoryBytes},
                      base::as_byte_span(decrypted_pin), salt,
                      base::as_writable_byte_span(hash_bytes),
                      MakeCryptoPassKey());

  crypto::ProcessBoundString bound_hash(hash_bytes);
  crypto::SecureZeroBuffer(base::as_writable_byte_span(hash_bytes));

  return trusted_vault::GpmPinRecoveryFactor::HashedPin(
      std::move(bound_hash), std::move(salt), kDefaultScryptDifficulty,
      is_six_digits ? trusted_vault::GpmPinRecoveryFactor::PinForm::kSixDigits
                    : trusted_vault::GpmPinRecoveryFactor::PinForm::kArbitrary);
}

GpmPinVerificationResult VerifyGpmPinSlowly(
    const GpmPin& pin,
    const trusted_vault::GpmPinRecoveryFactor::HashedPin& expected) {
  base::AssertLongCPUWorkAllowed();
  CHECK(!pin->empty());
  if (expected.scrypt_hash.size() != kScryptHashSize ||
      expected.hash_salt.empty() || expected.hash_difficulty < 2 ||
      expected.hash_difficulty > kMaxScryptDifficulty ||
      !std::has_single_bit(static_cast<uint64_t>(expected.hash_difficulty))) {
    return GpmPinVerificationResult::kInvalidParameters;
  }

  crypto::SecureString decrypted_pin = pin->secure_value();
  std::array<uint8_t, kScryptHashSize> computed_hash;
  crypto::kdf::Scrypt({.cost = static_cast<uint64_t>(expected.hash_difficulty),
                       .block_size = 8,
                       .parallelization = 1,
                       .max_memory_bytes = kScryptMaxMemoryBytes},
                      base::as_byte_span(decrypted_pin), expected.hash_salt,
                      computed_hash, MakeCryptoPassKey());

  crypto::SecureString expected_hash = expected.scrypt_hash.secure_value();
  const bool matches =
      crypto::SecureMemEqual(computed_hash, base::as_byte_span(expected_hash));
  crypto::SecureZeroBuffer(computed_hash);
  return matches ? GpmPinVerificationResult::kMatch
                 : GpmPinVerificationResult::kMismatch;
}

}  // namespace webauthn
