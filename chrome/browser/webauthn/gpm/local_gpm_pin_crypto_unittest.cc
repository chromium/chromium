// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm/local_gpm_pin_crypto.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "chrome/browser/webauthn/gpm/gpm_pin.h"
#include "chrome/browser/webauthn/gpm_pin_crypto.h"
#include "components/trusted_vault/gpm_pin_recovery_factor.h"
#include "crypto/kdf.h"
#include "crypto/process_bound_string.h"
#include "crypto/subtle_passkey.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {
namespace {

using HashedPin = trusted_vault::GpmPinRecoveryFactor::HashedPin;
using PinForm = trusted_vault::GpmPinRecoveryFactor::PinForm;

GpmPin MakePin(const std::string& pin) {
  return GpmPin(crypto::ProcessBoundString(pin));
}

HashedPin MakeHashedPinWithDifficulty(std::string_view pin,
                                      std::vector<uint8_t> salt,
                                      int difficulty,
                                      PinForm pin_form) {
  std::array<uint8_t, 32> derived_hash;
  crypto::kdf::Scrypt({.cost = static_cast<uint64_t>(difficulty),
                       .block_size = 8,
                       .parallelization = 1},
                      base::as_byte_span(pin), salt, derived_hash,
                      crypto::SubtlePassKey::ForTesting());
  return HashedPin(crypto::ProcessBoundString(
                       std::string(base::as_string_view(derived_hash))),
                   std::move(salt), difficulty, pin_form);
}

TEST(LocalGpmPinCryptoTest, HashGpmPinSlowlySixDigitAndVerify) {
  const GpmPin pin = MakePin("123456");
  const HashedPin hashed = HashGpmPinSlowly(pin);

  EXPECT_EQ(hashed.scrypt_hash.size(), 32u);
  EXPECT_EQ(hashed.hash_salt.size(), 16u);
  EXPECT_EQ(hashed.hash_difficulty, 16384);
  EXPECT_EQ(hashed.pin_form, PinForm::kSixDigits);

  EXPECT_EQ(VerifyGpmPinSlowly(pin, hashed), GpmPinVerificationResult::kMatch);
  EXPECT_EQ(VerifyGpmPinSlowly(MakePin("654321"), hashed),
            GpmPinVerificationResult::kMismatch);
  EXPECT_EQ(VerifyGpmPinSlowly(MakePin("1234567"), hashed),
            GpmPinVerificationResult::kMismatch);
}

TEST(LocalGpmPinCryptoTest, HashGpmPinSlowlyInfersArbitraryPinForm) {
  for (const std::string& raw_pin :
       {"12345", "1234567", "12345a", "correct-horse"}) {
    const GpmPin pin = MakePin(raw_pin);
    const HashedPin hashed = HashGpmPinSlowly(pin);
    EXPECT_EQ(hashed.pin_form, PinForm::kArbitrary) << raw_pin;
    EXPECT_EQ(VerifyGpmPinSlowly(pin, hashed), GpmPinVerificationResult::kMatch)
        << raw_pin;
  }
}

TEST(LocalGpmPinCryptoTest, HashGpmPinSlowlyUsesRandomSalts) {
  const GpmPin pin = MakePin("123456");
  const HashedPin first = HashGpmPinSlowly(pin);
  const HashedPin second = HashGpmPinSlowly(pin);

  EXPECT_NE(first.hash_salt, second.hash_salt);
  EXPECT_NE(first.scrypt_hash.value(), second.scrypt_hash.value());
  EXPECT_EQ(VerifyGpmPinSlowly(pin, first), GpmPinVerificationResult::kMatch);
  EXPECT_EQ(VerifyGpmPinSlowly(pin, second), GpmPinVerificationResult::kMatch);
}

TEST(LocalGpmPinCryptoTest, VerifyGpmPinSlowlyAcceptsOtherValidDifficulties) {
  const std::vector<uint8_t> salt(16, 0x42);
  // Matches Android's N=4096 scrypt work factor as well as the minimum valid
  // power-of-two difficulty (N=2).
  for (int difficulty : {2, 4096}) {
    const HashedPin hashed = MakeHashedPinWithDifficulty(
        "654321", salt, difficulty, PinForm::kSixDigits);
    EXPECT_EQ(VerifyGpmPinSlowly(MakePin("654321"), hashed),
              GpmPinVerificationResult::kMatch)
        << "difficulty=" << difficulty;
    EXPECT_EQ(VerifyGpmPinSlowly(MakePin("123456"), hashed),
              GpmPinVerificationResult::kMismatch)
        << "difficulty=" << difficulty;
  }
}

TEST(LocalGpmPinCryptoTest, VerifyGpmPinSlowlyRejectsInvalidParameters) {
  const GpmPin pin = MakePin("123456");
  const std::vector<uint8_t> salt(16, 0xAB);
  const std::string valid_hash(32, 'x');

  // Empty salt.
  EXPECT_EQ(
      VerifyGpmPinSlowly(pin, HashedPin(crypto::ProcessBoundString(valid_hash),
                                        {}, 16384, PinForm::kSixDigits)),
      GpmPinVerificationResult::kInvalidParameters);

  // Wrong hash length.
  for (size_t bad_size : {0u, 31u, 33u}) {
    EXPECT_EQ(
        VerifyGpmPinSlowly(pin, HashedPin(crypto::ProcessBoundString(
                                              std::string(bad_size, 'x')),
                                          salt, 16384, PinForm::kSixDigits)),
        GpmPinVerificationResult::kInvalidParameters)
        << "size=" << bad_size;
  }

  // Invalid scrypt difficulties (non-positive, < 2, non-power-of-two, or
  // exceeding the 256 MiB memory cap).
  for (int bad_difficulty : {-1, 0, 1, 3, 10000, 1 << 18}) {
    EXPECT_EQ(VerifyGpmPinSlowly(
                  pin, HashedPin(crypto::ProcessBoundString(valid_hash), salt,
                                 bad_difficulty, PinForm::kSixDigits)),
              GpmPinVerificationResult::kInvalidParameters)
        << "difficulty=" << bad_difficulty;
  }
}

TEST(LocalGpmPinCryptoTest, VerifyGpmPinSlowlyMatchesRfc7914TestVector) {
  // RFC 7914 Section 12 test vector:
  // P = "pleaseletmein", S = "SodiumChloride", N = 16384, r = 8, p = 1,
  // dkLen = 32 (first 32 bytes of the 64-byte RFC vector).
  constexpr std::array<uint8_t, 32> kExpectedHash = {
      0x70, 0x23, 0xbd, 0xcb, 0x3a, 0xfd, 0x73, 0x48, 0x46, 0x1c, 0x06,
      0xcd, 0x81, 0xfd, 0x38, 0xeb, 0xfd, 0xa8, 0xfb, 0xba, 0x90, 0x4f,
      0x8e, 0x3e, 0xa9, 0xb5, 0x43, 0xf6, 0x54, 0x5d, 0xa1, 0xf2};
  constexpr std::string_view kSalt = "SodiumChloride";
  const HashedPin expected(crypto::ProcessBoundString(std::string(
                               base::as_string_view(kExpectedHash))),
                           std::vector<uint8_t>(kSalt.begin(), kSalt.end()),
                           16384, PinForm::kArbitrary);

  EXPECT_EQ(VerifyGpmPinSlowly(MakePin("pleaseletmein"), expected),
            GpmPinVerificationResult::kMatch);
  EXPECT_EQ(VerifyGpmPinSlowly(MakePin("pleaseletmeout"), expected),
            GpmPinVerificationResult::kMismatch);
}

TEST(LocalGpmPinCryptoTest, VerifyGpmPinSlowlyInteroperatesWithEnclaveHashing) {
  for (const std::string& raw_pin : {"123456", "correct-horse"}) {
    std::unique_ptr<HashedPIN> enclave_hashed = HashPINSlowly(raw_pin);
    ASSERT_TRUE(enclave_hashed);

    const HashedPin converted(
        crypto::ProcessBoundString(
            std::string(base::as_string_view(enclave_hashed->hashed))),
        std::vector<uint8_t>(enclave_hashed->metadata.salt.begin(),
                             enclave_hashed->metadata.salt.end()),
        enclave_hashed->metadata.n,
        enclave_hashed->metadata.is_six_digits ? PinForm::kSixDigits
                                               : PinForm::kArbitrary);

    EXPECT_EQ(VerifyGpmPinSlowly(MakePin(raw_pin), converted),
              GpmPinVerificationResult::kMatch)
        << raw_pin;
  }
}

}  // namespace
}  // namespace webauthn
