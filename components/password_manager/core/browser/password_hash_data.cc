// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_hash_data.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "crypto/openssl_util.h"
#include "crypto/random.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace password_manager {

namespace {

std::string CreateRandomSalt() {
  constexpr size_t kSyncPasswordSaltLength = 16;

  char buffer[kSyncPasswordSaltLength];
  crypto::RandBytes(buffer, kSyncPasswordSaltLength);
  // Explicit std::string constructor with a string length must be used in order
  // to avoid treating '\0' symbols as a string ends.
  std::string result(buffer, kSyncPasswordSaltLength);
  return result;
}

}  // namespace

PasswordHashData::PasswordHashData() = default;

PasswordHashData::PasswordHashData(const PasswordHashData& other) = default;

PasswordHashData::PasswordHashData(const std::string& username,
                                   const base::string16& password,
                                   bool force_update,
                                   bool is_gaia_password)
    : username(username),
      length(password.size()),
      salt(CreateRandomSalt()),
      hash(CalculatePasswordHash(password, salt)),
      force_update(force_update),
      is_gaia_password(is_gaia_password) {}

bool PasswordHashData::MatchesPassword(const std::string& username,
                                       const base::string16& password,
                                       bool is_gaia_password) const {
  if (password.size() != this->length ||
      !AreUsernamesSame(username, is_gaia_password, this->username,
                        this->is_gaia_password)) {
    return false;
  }

  return CalculatePasswordHash(password, this->salt) == this->hash;
}

uint64_t CalculatePasswordHash(const base::StringPiece16& text,
                               const std::string& salt) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  constexpr size_t kBytesFromHash = 8;
  constexpr uint64_t kScryptCost = 32;  // It must be a power of 2.
  constexpr uint64_t kScryptBlockSize = 8;
  constexpr uint64_t kScryptParallelization = 1;
  constexpr size_t kScryptMaxMemory = 1024 * 1024;

  uint8_t hash[kBytesFromHash];
  base::StringPiece text_8bits(reinterpret_cast<const char*>(text.data()),
                               text.size() * 2);
  const uint8_t* salt_ptr = reinterpret_cast<const uint8_t*>(salt.c_str());

  int scrypt_ok = EVP_PBE_scrypt(text_8bits.data(), text_8bits.size(), salt_ptr,
                                 salt.size(), kScryptCost, kScryptBlockSize,
                                 kScryptParallelization, kScryptMaxMemory, hash,
                                 kBytesFromHash);

  // EVP_PBE_scrypt can only fail due to memory allocation error (which aborts
  // Chromium) or invalid parameters. In case of a failure a hash could leak
  // information from the stack, so using CHECK is better than DCHECK.
  CHECK(scrypt_ok);

  // Take 37 bits of |hash|.
  uint64_t hash37 = ((static_cast<uint64_t>(hash[0]))) |
                    ((static_cast<uint64_t>(hash[1])) << 8) |
                    ((static_cast<uint64_t>(hash[2])) << 16) |
                    ((static_cast<uint64_t>(hash[3])) << 24) |
                    (((static_cast<uint64_t>(hash[4])) & 0x1F) << 32);

  return hash37;
}

std::string CanonicalizeUsername(const std::string& username,
                                 bool is_gaia_account) {
  std::vector<std::string> parts = base::SplitString(
      username, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2U) {
    if (is_gaia_account && parts.size() == 1U)
      return gaia::CanonicalizeEmail(username + "@gmail.com");
    return username;
  }
  return gaia::CanonicalizeEmail(username);
}

bool AreUsernamesSame(const std::string& username1,
                      bool is_username1_gaia_account,
                      const std::string& username2,
                      bool is_username2_gaia_account) {
  if (is_username1_gaia_account != is_username2_gaia_account)
    return false;
  return CanonicalizeUsername(username1, is_username1_gaia_account) ==
         CanonicalizeUsername(username2, is_username2_gaia_account);
}

}  // namespace password_manager
