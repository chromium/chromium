// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"

#include <climits>
#include <optional>
#include <string_view>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "crypto/openssl_util.h"
#include "crypto/sha2.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/private-join-and-compute/src/crypto/ec_commutative_cipher.h"

namespace password_manager {

namespace {

template <typename T, typename CharT = typename T::value_type>
std::basic_string<CharT> CanonicalizeUsernameT(T username) {
  static constexpr CharT kPeriod = '.';

  std::basic_string<CharT> email_lower = base::ToLowerASCII(username);
  // |email_lower| might be an email address. Strip off the mail-address host,
  // remove periods from the username and return the result.
  std::basic_string<CharT> user_lower =
      email_lower.substr(0, email_lower.find_last_of('@'));
  base::RemoveChars(user_lower, {&kPeriod, 1}, &user_lower);
  return user_lower;
}

}  // namespace

std::string CanonicalizeUsername(std::string_view username) {
  return CanonicalizeUsernameT(username);
}

std::u16string CanonicalizeUsername(std::u16string_view username) {
  return CanonicalizeUsernameT(username);
}

std::string HashUsername(std::string_view canonicalized_username) {
  // Needs to stay in sync with server side constant: go/passwords-leak-salts
  static constexpr uint8_t kUsernameSalt[] = {
      0xC4, 0x94, 0xA3, 0x95, 0xF8, 0xC0, 0xE2, 0x3E, 0xA9, 0x23, 0x04,
      0x78, 0x70, 0x2C, 0x72, 0x18, 0x56, 0x54, 0x99, 0xB3, 0xE9, 0x21,
      0x18, 0x6C, 0x21, 0x1A, 0x01, 0x22, 0x3C, 0x45, 0x4A, 0xFA};

  // Check that |canonicalized_username| is actually canonicalized.
  // Note: We can't use CanonicalizeUsername() again, since it's not idempotent
  // if multiple '@' signs are present in the initial username.
  DCHECK_EQ(base::ToLowerASCII(canonicalized_username), canonicalized_username);
  return crypto::SHA256HashString(base::StrCat(
      {canonicalized_username,
       std::string_view(reinterpret_cast<const char*>(kUsernameSalt),
                        std::size(kUsernameSalt))}));
}

std::string BucketizeUsername(std::string_view canonicalized_username) {
  // Compute the number of bytes necessary to store `kUsernameHashPrefixLength`
  // bits.
  constexpr size_t kPrefixBytes =
      (kUsernameHashPrefixLength + CHAR_BIT - 1) / CHAR_BIT;
  // Compute the remainder, and construct a mask that keeps the first
  // `kPrefixRemainder` bits.
  constexpr size_t kPrefixRemainder = kUsernameHashPrefixLength % CHAR_BIT;
  constexpr size_t kPrefixMask = ((1 << kPrefixRemainder) - 1)
                                 << (CHAR_BIT - kPrefixRemainder);

  // Check that |canonicalized_username| is actually canonicalized.
  // Note: We can't use CanonicalizeUsername() again, since it's not idempotent
  // if multiple '@' signs are present in the initial username.
  DCHECK_EQ(base::ToLowerASCII(canonicalized_username), canonicalized_username);
  std::string prefix =
      HashUsername(canonicalized_username).substr(0, kPrefixBytes);
  if (kPrefixRemainder != 0) {
    prefix.back() &= kPrefixMask;
  }
  return prefix;
}

std::optional<std::string> ScryptHashUsernameAndPassword(
    std::string_view canonicalized_username,
    std::string_view password) {
  // Constant salt added to the password hash on top of canonicalized_username.
  // Needs to stay in sync with server side constant: go/passwords-leak-salts
  static constexpr uint8_t kPasswordHashSalt[] = {
      0x30, 0x76, 0x2A, 0xD2, 0x3F, 0x7B, 0xA1, 0x9B, 0xF8, 0xE3, 0x42,
      0xFC, 0xA1, 0xA7, 0x8D, 0x06, 0xE6, 0x6B, 0xE4, 0xDB, 0xB8, 0x4F,
      0x81, 0x53, 0xC5, 0x03, 0xC8, 0xDB, 0xBd, 0xDE, 0xA5, 0x20};
  static constexpr size_t kHashKeyLength = 32;
  static constexpr uint64_t kScryptCost = 1 << 12;  // It must be a power of 2.
  static constexpr uint64_t kScryptBlockSize = 8;
  static constexpr uint64_t kScryptParallelization = 1;
  static constexpr size_t kScryptMaxMemory = 1024 * 1024 * 32;

  // Check that |canonicalized_username| is actually canonicalized.
  // Note: We can't use CanonicalizeUsername() again, since it's not idempotent
  // if multiple '@' signs are present in the initial username.
  DCHECK_EQ(base::ToLowerASCII(canonicalized_username), canonicalized_username);
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  std::string username_password =
      base::StrCat({canonicalized_username, password});
  std::string salt = base::StrCat(
      {canonicalized_username,
       std::string_view(reinterpret_cast<const char*>(kPasswordHashSalt),
                        std::size(kPasswordHashSalt))});

  std::string result;
  uint8_t* key_data =
      reinterpret_cast<uint8_t*>(base::WriteInto(&result, kHashKeyLength + 1));

  int scrypt_ok =
      EVP_PBE_scrypt(username_password.data(), username_password.size(),
                     reinterpret_cast<const uint8_t*>(salt.data()), salt.size(),
                     kScryptCost, kScryptBlockSize, kScryptParallelization,
                     kScryptMaxMemory, key_data, kHashKeyLength);
  return scrypt_ok == 1 ? std::make_optional(std::move(result)) : std::nullopt;
}

std::optional<std::string> CipherEncrypt(const std::string& plaintext,
                                         std::string* key) {
  using ::private_join_and_compute::ECCommutativeCipher;
  auto cipher = ECCommutativeCipher::CreateWithNewKey(
      NID_X9_62_prime256v1, ECCommutativeCipher::SHA256);
  if (cipher.ok()) {
    auto result = cipher.value()->Encrypt(plaintext);
    if (result.ok()) {
      *key = cipher.value()->GetPrivateKeyBytes();
      return std::move(result).value();
    }
  }
  return std::nullopt;
}

std::optional<std::string> CipherEncryptWithKey(const std::string& plaintext,
                                                const std::string& key) {
  using ::private_join_and_compute::ECCommutativeCipher;
  auto cipher = ECCommutativeCipher::CreateFromKey(NID_X9_62_prime256v1, key,
                                                   ECCommutativeCipher::SHA256);
  if (cipher.ok()) {
    auto result = cipher.value()->Encrypt(plaintext);
    if (result.ok()) {
      return std::move(result).value();
    }
  }
  return std::nullopt;
}

std::optional<std::string> CipherReEncrypt(const std::string& already_encrypted,
                                           std::string* key) {
  using ::private_join_and_compute::ECCommutativeCipher;
  auto cipher = ECCommutativeCipher::CreateWithNewKey(
      NID_X9_62_prime256v1, ECCommutativeCipher::SHA256);
  if (cipher.ok()) {
    auto result = cipher.value()->ReEncrypt(already_encrypted);
    if (result.ok()) {
      *key = cipher.value()->GetPrivateKeyBytes();
      return std::move(result).value();
    }
  }
  return std::nullopt;
}

std::optional<std::string> CipherDecrypt(const std::string& ciphertext,
                                         const std::string& key) {
  using ::private_join_and_compute::ECCommutativeCipher;
  auto cipher = ECCommutativeCipher::CreateFromKey(NID_X9_62_prime256v1, key,
                                                   ECCommutativeCipher::SHA256);
  if (cipher.ok()) {
    auto result = cipher.value()->Decrypt(ciphertext);
    if (result.ok()) {
      return std::move(result).value();
    }
  }
  return std::nullopt;
}

std::optional<std::string> CreateNewKey() {
  using ::private_join_and_compute::ECCommutativeCipher;
  auto cipher = ECCommutativeCipher::CreateWithNewKey(
      NID_X9_62_prime256v1, ECCommutativeCipher::SHA256);
  if (cipher.ok()) {
    return cipher.value()->GetPrivateKeyBytes();
  }
  return std::nullopt;
}

}  // namespace password_manager
