// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/os_crypt.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/cxx17_backports.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "components/os_crypt/key_storage_config_linux.h"
#include "components/os_crypt/key_storage_linux.h"
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

// Prefixes for cypher text returned by obfuscation version.  We prefix the
// ciphertext with this string so that future data migration can detect
// this and migrate to full encryption without data loss. kObfuscationPrefixV10
// means that the hardcoded password will be used. kObfuscationPrefixV11 means
// that a password is/will be stored using an OS-level library (e.g Libsecret).
// V11 will not be used if such a library is not available.
constexpr char kObfuscationPrefixV10[] = "v10";
constexpr char kObfuscationPrefixV11[] = "v11";

// Everything in Cache may be leaked on shutdown.
struct Cache {
  // For password_v10, null means uninitialised.
  std::unique_ptr<crypto::SymmetricKey> password_v10_cache;
  // For password_v11, null means no backend.
  std::unique_ptr<crypto::SymmetricKey> password_v11_cache;
  bool is_password_v11_cached = false;
  // |config| is used to initialise |password_v11_cache| and then cleared.
  std::unique_ptr<os_crypt::Config> config;
  // Guards access to |g_cache|, making lazy initialization of individual parts
  // thread safe.
  base::Lock lock;
};

base::LazyInstance<Cache>::Leaky g_cache = LAZY_INSTANCE_INITIALIZER;

// Create the KeyStorage. Will be null if no service is found. A Config must be
// set before every call to this function.
std::unique_ptr<KeyStorageLinux> CreateKeyStorage() {
  CHECK(g_cache.Get().config);
  std::unique_ptr<KeyStorageLinux> key_storage =
      KeyStorageLinux::CreateService(*g_cache.Get().config);
  g_cache.Get().config.reset();
  return key_storage;
}

// Pointer to a function that creates and returns the |KeyStorage| instance to
// be used. The function maintains ownership of the pointer.
std::unique_ptr<KeyStorageLinux> (*g_key_storage_provider)() =
    &CreateKeyStorage;

// Generates a newly allocated SymmetricKey object based on a password.
// Ownership of the key is passed to the caller. Returns null key if a key
// generation error occurs.
std::unique_ptr<crypto::SymmetricKey> GenerateEncryptionKey(
    const std::string& password) {
  const std::string salt(kSalt);

  // Create an encryption key from our password and salt.
  std::unique_ptr<crypto::SymmetricKey> encryption_key(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES, password, salt, kEncryptionIterations,
          kDerivedKeySizeInBits));
  DCHECK(encryption_key);

  return encryption_key;
}

// Returns a cached string of "peanuts". Is thread-safe.
crypto::SymmetricKey* GetPasswordV10() {
  base::AutoLock auto_lock(g_cache.Get().lock);
  if (!g_cache.Get().password_v10_cache.get()) {
    g_cache.Get().password_v10_cache = GenerateEncryptionKey("peanuts");
  }
  return g_cache.Get().password_v10_cache.get();
}

// Caches and returns the password from the KeyStorage or null if there is no
// service. Is thread-safe.
crypto::SymmetricKey* GetPasswordV11() {
  base::AutoLock auto_lock(g_cache.Get().lock);
  if (!g_cache.Get().is_password_v11_cached) {
    std::unique_ptr<KeyStorageLinux> key_storage = g_key_storage_provider();
    if (key_storage) {
      absl::optional<std::string> key = key_storage->GetKey();
      if (key.has_value()) {
        g_cache.Get().password_v11_cache = GenerateEncryptionKey(*key);
      }
    }
    g_cache.Get().is_password_v11_cached = true;
  }
  return g_cache.Get().password_v11_cache.get();
}

}  // namespace

namespace OSCrypt {
void SetConfig(std::unique_ptr<os_crypt::Config> config) {
  OSCryptImpl::SetConfig(std::move(config));
}
bool EncryptString16(const std::u16string& plaintext, std::string* ciphertext) {
  return OSCryptImpl::EncryptString16(plaintext, ciphertext);
}
bool DecryptString16(const std::string& ciphertext, std::u16string* plaintext) {
  return OSCryptImpl::DecryptString16(ciphertext, plaintext);
}
bool EncryptString(const std::string& plaintext, std::string* ciphertext) {
  return OSCryptImpl::EncryptString(plaintext, ciphertext);
}
bool DecryptString(const std::string& ciphertext, std::string* plaintext) {
  return OSCryptImpl::DecryptString(ciphertext, plaintext);
}
std::string GetRawEncryptionKey() {
  return OSCryptImpl::GetRawEncryptionKey();
}
void SetRawEncryptionKey(const std::string& key) {
  OSCryptImpl::SetRawEncryptionKey(key);
}
bool IsEncryptionAvailable() {
  return OSCryptImpl::IsEncryptionAvailable();
}
void UseMockKeyStorageForTesting(
    std::unique_ptr<KeyStorageLinux> (*get_key_storage_mock)()) {
  OSCryptImpl::UseMockKeyStorageForTesting(std::move(get_key_storage_mock));
}
void ClearCacheForTesting() {
  OSCryptImpl::ClearCacheForTesting();
}
void SetEncryptionPasswordForTesting(const std::string& password) {
  OSCryptImpl::SetEncryptionPasswordForTesting(password);
}
}  // namespace OSCrypt

// static
bool OSCryptImpl::EncryptString16(const std::u16string& plaintext,
                              std::string* ciphertext) {
  return EncryptString(base::UTF16ToUTF8(plaintext), ciphertext);
}

// static
bool OSCryptImpl::DecryptString16(const std::string& ciphertext,
                              std::u16string* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}

// static
bool OSCryptImpl::EncryptString(const std::string& plaintext,
                            std::string* ciphertext) {
  if (plaintext.empty()) {
    ciphertext->clear();
    return true;
  }

  // If we are able to create a V11 key (i.e. a KeyStorage was available), then
  // we'll use it. If not, we'll use V10.
  crypto::SymmetricKey* encryption_key = GetPasswordV11();
  std::string obfuscation_prefix = kObfuscationPrefixV11;
  if (!encryption_key) {
    encryption_key = GetPasswordV10();
    obfuscation_prefix = kObfuscationPrefixV10;
  }

  if (!encryption_key)
    return false;

  const std::string iv(kIVBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key, crypto::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Encrypt(plaintext, ciphertext))
    return false;

  // Prefix the cipher text with version information.
  ciphertext->insert(0, obfuscation_prefix);
  return true;
}

// static
bool OSCryptImpl::DecryptString(const std::string& ciphertext,
                            std::string* plaintext) {
  if (ciphertext.empty()) {
    plaintext->clear();
    return true;
  }

  // Check that the incoming ciphertext was encrypted and with what version.
  // Credit card numbers are current legacy unencrypted data, so false match
  // with prefix won't happen.
  crypto::SymmetricKey* encryption_key = nullptr;
  std::string obfuscation_prefix;
  if (base::StartsWith(ciphertext, kObfuscationPrefixV10,
                       base::CompareCase::SENSITIVE)) {
    encryption_key = GetPasswordV10();
    obfuscation_prefix = kObfuscationPrefixV10;
  } else if (base::StartsWith(ciphertext, kObfuscationPrefixV11,
                              base::CompareCase::SENSITIVE)) {
    encryption_key = GetPasswordV11();
    obfuscation_prefix = kObfuscationPrefixV11;
  } else {
    // If the prefix is not found then we'll assume we're dealing with
    // old data saved as clear text and we'll return it directly.
    *plaintext = ciphertext;
    return true;
  }

  if (!encryption_key) {
    VLOG(1) << "Decryption failed: could not get the key";
    return false;
  }

  const std::string iv(kIVBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key, crypto::Encryptor::CBC, iv))
    return false;

  // Strip off the versioning prefix before decrypting.
  const std::string raw_ciphertext =
      ciphertext.substr(obfuscation_prefix.length());

  if (!encryptor.Decrypt(raw_ciphertext, plaintext)) {
    VLOG(1) << "Decryption failed";
    return false;
  }

  return true;
}

// static
void OSCryptImpl::SetConfig(std::unique_ptr<os_crypt::Config> config) {
  // Setting initialisation parameters makes no sense after initializing.
  DCHECK(!g_cache.Get().is_password_v11_cached);
  g_cache.Get().config = std::move(config);
}

// static
bool OSCryptImpl::IsEncryptionAvailable() {
  return GetPasswordV11();
}

// static
void OSCryptImpl::SetRawEncryptionKey(const std::string& raw_key) {
  base::AutoLock auto_lock(g_cache.Get().lock);
  // Check if the v11 password is already cached. If it is, then data encrypted
  // with the old password might not be decryptable.
  DCHECK(!g_cache.Get().is_password_v11_cached);
  // The config won't be used if this function is being called. Callers should
  // choose between setting a config and setting a raw encryption key.
  DCHECK(!g_cache.Get().config);
  if (!raw_key.empty()) {
    g_cache.Get().password_v11_cache =
        crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, raw_key);
  }
  // Always set |is_password_v11_cached|, even if given an empty string.
  // Note that |raw_key| can be an empty string if real V11 encryption is not
  // available, and setting |is_password_v11_cached| causes GetPasswordV11() to
  // correctly return nullptr in that case.
  g_cache.Get().is_password_v11_cached = true;
}

// static
std::string OSCryptImpl::GetRawEncryptionKey() {
  if (crypto::SymmetricKey* key = GetPasswordV11())
    return key->key();
  return std::string();
}

// static
void OSCryptImpl::ClearCacheForTesting() {
  g_cache.Get().password_v10_cache.reset();
  g_cache.Get().password_v11_cache.reset();
  g_cache.Get().is_password_v11_cached = false;
  g_cache.Get().config.reset();
}

// static
void OSCryptImpl::UseMockKeyStorageForTesting(
    std::unique_ptr<KeyStorageLinux> (*get_key_storage_mock)()) {
  if (get_key_storage_mock)
    g_key_storage_provider = get_key_storage_mock;
  else
    g_key_storage_provider = &CreateKeyStorage;
}

// static
void OSCryptImpl::SetEncryptionPasswordForTesting(const std::string& password) {
  ClearCacheForTesting();  // IN-TEST
  g_cache.Get().password_v11_cache = GenerateEncryptionKey(password);
  g_cache.Get().is_password_v11_cached = true;
}
