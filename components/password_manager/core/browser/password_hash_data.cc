// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_hash_data.h"

#include <iterator>
#include <string_view>

#include "base/numerics/byte_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "crypto/kdf.h"
#include "crypto/openssl_util.h"
#include "crypto/random.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace password_manager {

crypto::SubtlePassKey MakeCryptoPassKeyForPasswordHash() {
  return {};
}

PasswordHashData::PasswordHashData() = default;

PasswordHashData::PasswordHashData(const PasswordHashData& other) = default;

PasswordHashData& PasswordHashData::operator=(const PasswordHashData& other) =
    default;

PasswordHashData::PasswordHashData(const std::string& username,
                                   const std::u16string& password,
                                   bool force_update,
                                   bool is_gaia_password)
    : username(username),
      length(password.size()),
      salt(CreateRandomSalt()),
      hash(CalculatePasswordHash(password, salt)),
      force_update(force_update),
      is_gaia_password(is_gaia_password) {}

bool PasswordHashData::MatchesPassword(const std::string& user,
                                       const std::u16string& pass,
                                       bool is_gaia_pass) const {
  if (pass.size() != length ||
      !AreUsernamesSame(user, is_gaia_pass, username, is_gaia_password)) {
    return false;
  }

  return CalculatePasswordHash(pass, salt) == hash;
}

uint64_t CalculatePasswordHash(std::u16string_view text,
                               const std::string& salt) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  constexpr crypto::kdf::ScryptParams kScryptParams = {
      .cost = 32,  // scrypt requires this to be a power of 2
      .block_size = 8,
      .parallelization = 1,
      .max_memory_bytes = 1024 * 1024,
  };

  std::array<uint8_t, 8> result;
  crypto::kdf::DeriveKeyScrypt(kScryptParams, base::as_byte_span(text),
                               base::as_byte_span(salt), result,
                               MakeCryptoPassKeyForPasswordHash());

  uint64_t val = base::U64FromLittleEndian(result);
  // Take 37 bits of |hash|.
  return val & UINT64_C(0x1FFFFFFFFF);
}

std::string CanonicalizeUsername(const std::string& username,
                                 bool is_gaia_account) {
  std::vector<std::string> parts = base::SplitString(
      username, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2U) {
    if (is_gaia_account && parts.size() == 1U) {
      return gaia::CanonicalizeEmail(username + "@gmail.com");
    }
    return username;
  }
  return gaia::CanonicalizeEmail(username);
}

bool AreUsernamesSame(const std::string& username1,
                      bool is_username1_gaia_account,
                      const std::string& username2,
                      bool is_username2_gaia_account) {
  if (is_username1_gaia_account != is_username2_gaia_account) {
    return false;
  }
  return CanonicalizeUsername(username1, is_username1_gaia_account) ==
         CanonicalizeUsername(username2, is_username2_gaia_account);
}

std::string CreateRandomSalt() {
  constexpr size_t kSyncPasswordSaltLength = 16;

  uint8_t buffer[kSyncPasswordSaltLength];
  crypto::RandBytes(buffer);
  // Explicit std::string constructor with a string length must be used in order
  // to avoid treating '\0' symbols as a string ends.
  return std::string(std::begin(buffer), std::end(buffer));
}

}  // namespace password_manager
