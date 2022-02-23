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
const char kSalt[] = "saltysalt";

// Key size required for 128 bit AES.
const size_t kDerivedKeySizeInBits = 128;

// Constant for Symmetic key derivation.
const size_t kEncryptionIterations = 1;

// Size of initialization vector for AES 128-bit.
const size_t kIVBlockSizeAES128 = 16;

// Password version. V10 means that the hardcoded password will be used.
// V11 means that a password is/will be stored using an OS-level library (e.g
// Libsecret). V11 will not be used if such a library is not available.
// Used for array indexing.
enum Version {
  V10 = 0,
  V11 = 1,
};

// Prefix for cipher text returned by obfuscation version.  We prefix the
// ciphertext with this string so that future data migration can detect
// this and migrate to full encryption without data loss.
const char kObfuscationPrefix[][4] = {
    "v10", "v11",
};

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
  std::string salt(kSalt);

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

// Pointers to functions that return a password for deriving the encryption key.
// One function for each supported password version (see Version enum).
crypto::SymmetricKey* (*g_get_password[])() = {
    &GetPasswordV10,
    &GetPasswordV11,
};

}  // namespace

// static
bool OSCrypt::EncryptString16(const std::u16string& plaintext,
                              std::string* ciphertext) {
  return EncryptString(base::UTF16ToUTF8(plaintext), ciphertext);
}

// static
bool OSCrypt::DecryptString16(const std::string& ciphertext,
                              std::u16string* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}

// static
bool OSCrypt::EncryptString(const std::string& plaintext,
                            std::string* ciphertext) {
  if (plaintext.empty()) {
    ciphertext->clear();
    return true;
  }

  // If we are able to create a V11 key (i.e. a KeyStorage was available), then
  // we'll use it. If not, we'll use V10.
  Version version = Version::V11;
  crypto::SymmetricKey* encryption_key = g_get_password[version]();
  if (!encryption_key) {
    version = Version::V10;
    encryption_key = g_get_password[version]();
  }

  if (!encryption_key)
    return false;

  std::string iv(kIVBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key, crypto::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Encrypt(plaintext, ciphertext))
    return false;

  // Prefix the cipher text with version information.
  ciphertext->insert(0, kObfuscationPrefix[version]);
  return true;
}

// static
bool OSCrypt::DecryptString(const std::string& ciphertext,
                            std::string* plaintext) {
  if (ciphertext.empty()) {
    plaintext->clear();
    return true;
  }

  // Check that the incoming ciphertext was encrypted and with what version.
  // Credit card numbers are current legacy unencrypted data, so false match
  // with prefix won't happen.
  Version version;
  if (base::StartsWith(ciphertext, kObfuscationPrefix[Version::V10],
                       base::CompareCase::SENSITIVE)) {
    version = Version::V10;
  } else if (base::StartsWith(ciphertext, kObfuscationPrefix[Version::V11],
                              base::CompareCase::SENSITIVE)) {
    version = Version::V11;
  } else {
    // If the prefix is not found then we'll assume we're dealing with
    // old data saved as clear text and we'll return it directly.
    *plaintext = ciphertext;
    return true;
  }

  crypto::SymmetricKey* encryption_key(g_get_password[version]());
  if (!encryption_key) {
    VLOG(1) << "Decryption failed: could not get the key";
    return false;
  }

  std::string iv(kIVBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key, crypto::Encryptor::CBC, iv))
    return false;

  // Strip off the versioning prefix before decrypting.
  std::string raw_ciphertext =
      ciphertext.substr(strlen(kObfuscationPrefix[version]));

  if (!encryptor.Decrypt(raw_ciphertext, plaintext)) {
    VLOG(1) << "Decryption failed";
    return false;
  }

  return true;
}

// static
void OSCrypt::SetConfig(std::unique_ptr<os_crypt::Config> config) {
  // Setting initialisation parameters makes no sense after initializing.
  DCHECK(!g_cache.Get().is_password_v11_cached);
  g_cache.Get().config = std::move(config);
}

// static
bool OSCrypt::IsEncryptionAvailable() {
  return g_get_password[Version::V11]();
}

// static
void OSCrypt::SetRawEncryptionKey(const std::string& raw_key) {
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
std::string OSCrypt::GetRawEncryptionKey() {
  crypto::SymmetricKey* key = g_get_password[Version::V11]();
  if (!key)
    return std::string();
  return key->key();
}

// static
void OSCrypt::ClearCacheForTesting() {
  g_cache.Get().password_v10_cache.reset();
  g_cache.Get().password_v11_cache.reset();
  g_cache.Get().is_password_v11_cached = false;
  g_cache.Get().config.reset();
}

// static
void OSCrypt::UseMockKeyStorageForTesting(
    std::unique_ptr<KeyStorageLinux> (*get_key_storage_mock)()) {
  if (get_key_storage_mock)
    g_key_storage_provider = get_key_storage_mock;
  else
    g_key_storage_provider = &CreateKeyStorage;
}

// static
void OSCrypt::SetEncryptionPasswordForTesting(const std::string& password) {
  ClearCacheForTesting();  // IN-TEST
  g_cache.Get().password_v11_cache = GenerateEncryptionKey(password);
  g_cache.Get().is_password_v11_cached = true;
}
