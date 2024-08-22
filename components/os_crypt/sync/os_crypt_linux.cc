// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "components/os_crypt/sync/key_storage_config_linux.h"
#include "components/os_crypt/sync/key_storage_linux.h"
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

// The UMA metric name for whether the false was decryptable with an empty key.
constexpr char kMetricDecryptedWithEmptyKey[] =
    "OSCrypt.Linux.DecryptedWithEmptyKey";

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

// Decrypt `ciphertext` using `encryption_key` and store the result in
// `encryption_key`.
bool DecryptWith(const std::string& ciphertext,
                 crypto::SymmetricKey* encryption_key,
                 std::string* plaintext) {
  const std::string iv(kIVBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key, crypto::Encryptor::CBC, iv)) {
    return false;
  }

  return encryptor.Decrypt(ciphertext, plaintext);
}

}  // namespace

namespace OSCrypt {
void SetConfig(std::unique_ptr<os_crypt::Config> config) {
  OSCryptImpl::GetInstance()->SetConfig(std::move(config));
}
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
std::string GetRawEncryptionKey() {
  return OSCryptImpl::GetInstance()->GetRawEncryptionKey();
}
void SetRawEncryptionKey(const std::string& key) {
  OSCryptImpl::GetInstance()->SetRawEncryptionKey(key);
}
bool IsEncryptionAvailable() {
  return OSCryptImpl::GetInstance()->IsEncryptionAvailable();
}
void UseMockKeyStorageForTesting(
    base::OnceCallback<std::unique_ptr<KeyStorageLinux>()>
        storage_provider_factory) {
  OSCryptImpl::GetInstance()->UseMockKeyStorageForTesting(
      std::move(storage_provider_factory));
}
void ClearCacheForTesting() {
  OSCryptImpl::GetInstance()->ClearCacheForTesting();
}
void SetEncryptionPasswordForTesting(const std::string& password) {
  OSCryptImpl::GetInstance()->SetEncryptionPasswordForTesting(password);
}
}  // namespace OSCrypt

OSCryptImpl* OSCryptImpl::GetInstance() {
  return base::Singleton<OSCryptImpl,
                         base::LeakySingletonTraits<OSCryptImpl>>::get();
}

OSCryptImpl::OSCryptImpl() = default;

OSCryptImpl::~OSCryptImpl() = default;

bool OSCryptImpl::EncryptString16(const std::u16string& plaintext,
                                  std::string* ciphertext) {
  return EncryptString(base::UTF16ToUTF8(plaintext), ciphertext);
}

bool OSCryptImpl::DecryptString16(const std::string& ciphertext,
                                  std::u16string* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8)) {
    return false;
  }

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}

bool OSCryptImpl::EncryptString(const std::string& plaintext,
                                std::string* ciphertext) {
  if (plaintext.empty()) {
    ciphertext->clear();
    return true;
  }

  // If we are able to create a V11 key (i.e. a KeyStorage was available), then
  // we'll use it. If not, we'll use V10.
  crypto::SymmetricKey* encryption_key = GetPasswordV11(/*probe=*/false);
  std::string obfuscation_prefix = kObfuscationPrefixV11;
  if (!encryption_key) {
    encryption_key = GetPasswordV10();
    obfuscation_prefix = kObfuscationPrefixV10;
  }

  if (!encryption_key) {
    return false;
  }

  const std::string iv(kIVBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key, crypto::Encryptor::CBC, iv)) {
    return false;
  }

  if (!encryptor.Encrypt(plaintext, ciphertext)) {
    return false;
  }

  // Prefix the cipher text with version information.
  ciphertext->insert(0, obfuscation_prefix);
  return true;
}

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
    encryption_key = GetPasswordV11(/*probe=*/false);
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

  // Strip off the versioning prefix before decrypting.
  const std::string raw_ciphertext =
      ciphertext.substr(obfuscation_prefix.length());

  if (DecryptWith(raw_ciphertext, encryption_key, plaintext)) {
    base::UmaHistogramBoolean(kMetricDecryptedWithEmptyKey, false);
    return true;
  }

  // Some clients have encrypted data with an empty key. See
  // crbug.com/1195256.
  auto empty_key = GenerateEncryptionKey(std::string());
  if (DecryptWith(raw_ciphertext, empty_key.get(), plaintext)) {
    VLOG(1) << "Decryption succeeded after retrying with an empty key";
    base::UmaHistogramBoolean(kMetricDecryptedWithEmptyKey, true);
    return true;
  }

  VLOG(1) << "Decryption failed";
  base::UmaHistogramBoolean(kMetricDecryptedWithEmptyKey, false);
  return false;
}

void OSCryptImpl::SetConfig(std::unique_ptr<os_crypt::Config> config) {
  // Setting initialisation parameters makes no sense after initializing.
  DCHECK(!is_password_v11_cached_);
  config_ = std::move(config);
}

bool OSCryptImpl::IsEncryptionAvailable() {
  return GetPasswordV11(/*probe=*/true);
}

void OSCryptImpl::SetRawEncryptionKey(const std::string& raw_key) {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
  // Check if the v11 password is already cached. If it is, then data encrypted
  // with the old password might not be decryptable.
  DCHECK(!is_password_v11_cached_);
  // The config won't be used if this function is being called. Callers should
  // choose between setting a config and setting a raw encryption key.
  DCHECK(!config_);
  if (!raw_key.empty()) {
    password_v11_cache_ =
        crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, raw_key);
  }
  // Always set |is_password_v11_cached_|, even if given an empty string.
  // Note that |raw_key| can be an empty string if real V11 encryption is not
  // available, and setting |is_password_v11_cached_| causes GetPasswordV11 to
  // correctly return nullptr in that case.
  is_password_v11_cached_ = true;
}

std::string OSCryptImpl::GetRawEncryptionKey() {
  if (crypto::SymmetricKey* key = GetPasswordV11(/*probe=*/false)) {
    return key->key();
  }
  return std::string();
}

void OSCryptImpl::ClearCacheForTesting() {
  password_v10_cache_.reset();
  password_v11_cache_.reset();
  is_password_v11_cached_ = false;
  config_.reset();
}

void OSCryptImpl::UseMockKeyStorageForTesting(
    base::OnceCallback<std::unique_ptr<KeyStorageLinux>()>
        storage_provider_factory) {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
  storage_provider_factory_for_testing_ = std::move(storage_provider_factory);
}

void OSCryptImpl::SetEncryptionPasswordForTesting(const std::string& password) {
  ClearCacheForTesting();  // IN-TEST
  password_v11_cache_ = GenerateEncryptionKey(password);
  is_password_v11_cached_ = true;
}

// Returns a cached string of "peanuts". Is thread-safe.
crypto::SymmetricKey* OSCryptImpl::GetPasswordV10() {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
  if (!password_v10_cache_.get()) {
    password_v10_cache_ = GenerateEncryptionKey("peanuts");
  }
  return password_v10_cache_.get();
}

// Caches and returns the password from the KeyStorage or null if there is no
// service. Is thread-safe.
crypto::SymmetricKey* OSCryptImpl::GetPasswordV11(bool probe) {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
  if (is_password_v11_cached_) {
    return password_v11_cache_.get();
  }

  std::unique_ptr<KeyStorageLinux> key_storage;
  if (storage_provider_factory_for_testing_) {
    key_storage = std::move(storage_provider_factory_for_testing_).Run();
  } else {
    CHECK(probe || config_);
    if (config_) {
      key_storage = KeyStorageLinux::CreateService(*config_);
      config_.reset();
    }
  }

  if (key_storage) {
    std::optional<std::string> key = key_storage->GetKey();
    if (key.has_value()) {
      password_v11_cache_ = GenerateEncryptionKey(*key);
    }
  }

  is_password_v11_cached_ = true;
  return password_v11_cache_.get();
}

// static
base::Lock& OSCryptImpl::GetLock() {
  static base::NoDestructor<base::Lock> os_crypt_lock;
  return *os_crypt_lock;
}
