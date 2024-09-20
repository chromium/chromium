// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt.h"

#include <stddef.h>

#include <memory>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"

namespace {

// Salt for Symmetric key derivation.
constexpr char kSalt[] = "saltysalt";

// Key size required for 128 bit AES.
constexpr size_t kDerivedKeySizeInBits = 128;

// Constant for Symmetric key derivation.
constexpr size_t kEncryptionIterations = 1;

// Size of initialization vector for AES 128-bit.
constexpr size_t kIVBlockSizeAES128 = 16;

// Prefix for cypher text returned by obfuscation version.  We prefix the
// cyphertext with this string so that future data migration can detect
// this and migrate to full encryption without data loss.
constexpr char kObfuscationPrefix[] = "v10";

// Generates a newly allocated SymmetricKey object based a hard-coded password.
// Ownership of the key is passed to the caller.  Returns NULL key if a key
// generation error occurs.
crypto::SymmetricKey* GetEncryptionKey() {
  // We currently "obfuscate" by encrypting and decrypting with hard-coded
  // password.  We need to improve this password situation by moving a secure
  // password into a system-level key store.
  // http://crbug.com/25404 and http://crbug.com/49115
  const std::string password = "peanuts";
  const std::string salt(kSalt);

  // Create an encryption key from our password and salt.
  std::unique_ptr<crypto::SymmetricKey> encryption_key(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES, password, salt, kEncryptionIterations,
          kDerivedKeySizeInBits));
  DCHECK(encryption_key.get());

  return encryption_key.release();
}

}  // namespace

bool OSCrypt::EncryptString16(const std::u16string& plaintext,
                              std::string* ciphertext) {
  return EncryptString(base::UTF16ToUTF8(plaintext), ciphertext);
}

bool OSCrypt::DecryptString16(const std::string& ciphertext,
                              std::u16string* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}

bool OSCrypt::EncryptString(const std::string& plaintext,
                            std::string* ciphertext) {
  // This currently "obfuscates" by encrypting with hard-coded password.
  // We need to improve this password situation by moving a secure password
  // into a system-level key store.
  // http://crbug.com/25404 and http://crbug.com/49115

  if (plaintext.empty()) {
    *ciphertext = std::string();
    return true;
  }

  std::unique_ptr<crypto::SymmetricKey> encryption_key(GetEncryptionKey());
  if (!encryption_key.get())
    return false;

  const std::string iv(kIVBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key.get(), crypto::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Encrypt(plaintext, ciphertext))
    return false;

  // Prefix the cypher text with version information.
  ciphertext->insert(0, kObfuscationPrefix);
  return true;
}

bool OSCrypt::DecryptString(const std::string& ciphertext,
                            std::string* plaintext) {
  // This currently "obfuscates" by encrypting with hard-coded password.
  // We need to improve this password situation by moving a secure password
  // into a system-level key store.
  // http://crbug.com/25404 and http://crbug.com/49115

  if (ciphertext.empty()) {
    *plaintext = std::string();
    return true;
  }

  // Check that the incoming cyphertext was indeed encrypted with the expected
  // version.  If the prefix is not found then we'll assume we're dealing with
  // old data saved as clear text and we'll return it directly.
  // Credit card numbers are current legacy data, so false match with prefix
  // won't happen.
  const bool no_prefix_found = !base::StartsWith(ciphertext, kObfuscationPrefix,
                                                 base::CompareCase::SENSITIVE);

  base::UmaHistogramBoolean("OSCrypt.Posix.NoEncryptionPrefixFound",
                            no_prefix_found);

  if (no_prefix_found) {
    *plaintext = ciphertext;
    return true;
  }

  // Strip off the versioning prefix before decrypting.
  const std::string raw_ciphertext =
      ciphertext.substr(strlen(kObfuscationPrefix));

  std::unique_ptr<crypto::SymmetricKey> encryption_key(GetEncryptionKey());
  if (!encryption_key.get())
    return false;

  const std::string iv(kIVBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key.get(), crypto::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Decrypt(raw_ciphertext, plaintext))
    return false;

  return true;
}

// static
bool OSCrypt::IsEncryptionAvailable() {
  return false;
}

// static
void OSCrypt::SetRawEncryptionKey(const std::string& raw_key) {
  DCHECK(raw_key.empty());
}

// static
std::string OSCrypt::GetRawEncryptionKey() {
  return "";
}
