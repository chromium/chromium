// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt.h"

#include <CommonCrypto/CommonCryptor.h>  // for kCCBlockSizeAES128
#include <stddef.h>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/os_crypt/sync/keychain_password_mac.h"
#include "components/os_crypt/sync/os_crypt_switches.h"
#include "crypto/apple_keychain.h"
#include "crypto/encryptor.h"
#include "crypto/mock_apple_keychain.h"
#include "crypto/symmetric_key.h"

namespace os_crypt {
class EncryptionKeyCreationUtil;
}

namespace {

// Salt for Symmetric key derivation.
constexpr char kSalt[] = "saltysalt";

// Key size required for 128 bit AES.
constexpr size_t kDerivedKeySizeInBits = 128;

// Constant for Symmetic key derivation.
constexpr size_t kEncryptionIterations = 1003;

// Prefix for cypher text returned by current encryption version.  We prefix
// the cypher text with this string so that future data migration can detect
// this and migrate to different encryption without data loss.
constexpr char kEncryptionVersionPrefix[] = "v10";

}  // namespace

namespace OSCrypt {
bool EncryptString16(const std::u16string& plaintext, std::string* ciphertext) {
  return OSCryptImpl::GetInstance()->EncryptString16(plaintext, ciphertext);
}
bool DecryptString16(const std::string& ciphertext, std::u16string* plaintext) {
  return OSCryptImpl::GetInstance()->DecryptString16(ciphertext, plaintext);
}
bool EncryptString(const std::string& plaintext, std::string* ciphertext) {
  return OSCryptImpl::GetInstance()->EncryptString(plaintext, ciphertext);
}
bool DecryptString(const std::string& ciphertext, std::string* plaintext) {
  return OSCryptImpl::GetInstance()->DecryptString(ciphertext, plaintext);
}
void UseMockKeychainForTesting(bool use_mock) {
  OSCryptImpl::GetInstance()->UseMockKeychainForTesting(use_mock);
}
void UseLockedMockKeychainForTesting(bool use_locked) {
  OSCryptImpl::GetInstance()->UseLockedMockKeychainForTesting(use_locked);
}
std::string GetRawEncryptionKey() {
  return OSCryptImpl::GetInstance()->GetRawEncryptionKey();
}
void SetRawEncryptionKey(const std::string& key) {
  OSCryptImpl::GetInstance()->SetRawEncryptionKey(key);
}
bool IsEncryptionAvailable() {
  return OSCryptImpl::GetInstance()->IsEncryptionAvailable();
}
}  // namespace OSCrypt

// static
OSCryptImpl* OSCryptImpl::GetInstance() {
  return base::Singleton<OSCryptImpl,
                         base::LeakySingletonTraits<OSCryptImpl>>::get();
}

OSCryptImpl::OSCryptImpl() = default;
OSCryptImpl::~OSCryptImpl() = default;

// Generates a newly allocated SymmetricKey object based on the password found
// in the Keychain.  The generated key is for AES encryption.  Returns NULL key
// in the case password access is denied or key generation error occurs.
crypto::SymmetricKey* OSCryptImpl::GetEncryptionKey() {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());

  if (use_mock_keychain_ && use_locked_mock_keychain_)
    return nullptr;

  if (key_is_cached_)
    return cached_encryption_key_.get();

  const bool mock_keychain_command_line_flag =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          os_crypt::switches::kUseMockKeychain);

  std::string password;
  if (use_mock_keychain_ || mock_keychain_command_line_flag) {
    crypto::MockAppleKeychain keychain;
    password = keychain.GetEncryptionPassword();
  } else {
    crypto::AppleKeychain keychain;
    KeychainPassword encryptor_password(keychain);
    password = encryptor_password.GetPassword();
  }

  key_is_cached_ = true;
  if (password.empty())
    return cached_encryption_key_.get();

  const std::string salt(kSalt);

  // Create an encryption key from our password and salt.
  cached_encryption_key_ =
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES, password, salt, kEncryptionIterations,
          kDerivedKeySizeInBits);
  DCHECK(cached_encryption_key_);
  return cached_encryption_key_.get();
}

std::string OSCryptImpl::GetRawEncryptionKey() {
  if (crypto::SymmetricKey* key = GetEncryptionKey())
    return key->key();
  return std::string();
}

void OSCryptImpl::SetRawEncryptionKey(const std::string& raw_key) {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
  DCHECK(!cached_encryption_key_) << "Encryption key already set.";
  cached_encryption_key_ =
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, raw_key);
  key_is_cached_ = true;
}

bool OSCryptImpl::EncryptString16(const std::u16string& plaintext,
                              std::string* ciphertext) {
  return EncryptString(base::UTF16ToUTF8(plaintext), ciphertext);
}

bool OSCryptImpl::DecryptString16(const std::string& ciphertext,
                              std::u16string* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}

bool OSCryptImpl::EncryptString(const std::string& plaintext,
                            std::string* ciphertext) {
  if (plaintext.empty()) {
    *ciphertext = std::string();
    return true;
  }

  crypto::SymmetricKey* encryption_key = GetEncryptionKey();
  if (!encryption_key)
    return false;

  const std::string iv(kCCBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key, crypto::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Encrypt(plaintext, ciphertext))
    return false;

  // Prefix the cypher text with version information.
  ciphertext->insert(0, kEncryptionVersionPrefix);
  return true;
}

bool OSCryptImpl::DecryptString(const std::string& ciphertext,
                            std::string* plaintext) {
  if (ciphertext.empty()) {
    *plaintext = std::string();
    return true;
  }

  // Check that the incoming cyphertext was indeed encrypted with the expected
  // version.  If the prefix is not found then we'll assume we're dealing with
  // old data saved as clear text and we'll return it directly.
  // Credit card numbers are current legacy data, so false match with prefix
  // won't happen.
  if (ciphertext.find(kEncryptionVersionPrefix) != 0) {
    *plaintext = ciphertext;
    return true;
  }

  // Strip off the versioning prefix before decrypting.
  const std::string raw_ciphertext =
      ciphertext.substr(strlen(kEncryptionVersionPrefix));

  crypto::SymmetricKey* encryption_key = GetEncryptionKey();
  if (!encryption_key) {
    VLOG(1) << "Decryption failed: could not get the key";
    return false;
  }

  const std::string iv(kCCBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key, crypto::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Decrypt(raw_ciphertext, plaintext)) {
    VLOG(1) << "Decryption failed";
    return false;
  }

  return true;
}

bool OSCryptImpl::IsEncryptionAvailable() {
  return GetEncryptionKey() != nullptr;
}

void OSCryptImpl::UseMockKeychainForTesting(bool use_mock) {
  use_mock_keychain_ = use_mock;
  if (!use_mock_keychain_)
    use_locked_mock_keychain_ = false;
}

void OSCryptImpl::UseLockedMockKeychainForTesting(bool use_locked) {
  use_locked_mock_keychain_ = use_locked;
  if (use_locked_mock_keychain_)
    use_mock_keychain_ = true;
}

// static
base::Lock& OSCryptImpl::GetLock() {
  static base::NoDestructor<base::Lock> os_crypt_lock;
  return *os_crypt_lock;
}
