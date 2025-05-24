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
#include "components/os_crypt/sync/os_crypt_metrics.h"
#include "crypto/aes_cbc.h"
#include "crypto/kdf.h"

namespace {

// Prefixes for cypher text returned by obfuscation version.  We prefix the
// ciphertext with this string so that future data migration can detect
// this and migrate to full encryption without data loss. kObfuscationPrefixV10
// means that the hardcoded password will be used. kObfuscationPrefixV11 means
// that a password is/will be stored using an OS-level library (e.g Libsecret).
// V11 will not be used if such a library is not available.
constexpr char kObfuscationPrefixV10[] = "v10";
constexpr char kObfuscationPrefixV11[] = "v11";

constexpr crypto::kdf::Pbkdf2HmacSha1Params kParams{
    .iterations = 1,
};

const auto kSalt = base::byte_span_from_cstring("saltysalt");

// clang-format off
// PBKDF2-HMAC-SHA1(1 iteration, key = "peanuts", salt = "saltysalt")
constexpr auto kV10Key = std::to_array<uint8_t>({
    0xfd, 0x62, 0x1f, 0xe5, 0xa2, 0xb4, 0x02, 0x53,
    0x9d, 0xfa, 0x14, 0x7c, 0xa9, 0x27, 0x27, 0x78,
});

// PBKDF2-HMAC-SHA1(1 iteration, key = "", salt = "saltysalt")
constexpr auto kEmptyKey = std::to_array<uint8_t>({
    0xd0, 0xd0, 0xec, 0x9c, 0x7d, 0x77, 0xd4, 0x3a,
    0xc5, 0x41, 0x87, 0xfa, 0x48, 0x18, 0xd1, 0x7f,
});

const std::array<uint8_t, crypto::aes_cbc::kBlockSize> kIv{
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
};
// clang-format on

// The UMA metric name for whether the false was decryptable with an empty key.
constexpr char kMetricDecryptedWithEmptyKey[] =
    "OSCrypt.Linux.DecryptedWithEmptyKey";

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

  base::span<const uint8_t> key;

  if (DeriveV11Key()) {
    key = *v11_key_;
    *ciphertext = kObfuscationPrefixV11;
  } else {
    key = kV10Key;
    *ciphertext = kObfuscationPrefixV10;
  }

  ciphertext->append(base::as_string_view(
      crypto::aes_cbc::Encrypt(key, kIv, base::as_byte_span(plaintext))));

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
  base::span<const uint8_t> key;
  std::string obfuscation_prefix;
  os_crypt::EncryptionPrefixVersion encryption_version =
      os_crypt::EncryptionPrefixVersion::kNoVersion;

  if (base::StartsWith(ciphertext, kObfuscationPrefixV10,
                       base::CompareCase::SENSITIVE)) {
    key = kV10Key;
    obfuscation_prefix = kObfuscationPrefixV10;
    encryption_version = os_crypt::EncryptionPrefixVersion::kVersion10;
  } else if (base::StartsWith(ciphertext, kObfuscationPrefixV11,
                              base::CompareCase::SENSITIVE)) {
    if (!DeriveV11Key()) {
      VLOG(1) << "Decryption failed: could not get the key";
      return false;
    }
    key = *v11_key_;
    obfuscation_prefix = kObfuscationPrefixV11;
    encryption_version = os_crypt::EncryptionPrefixVersion::kVersion11;
  }

  os_crypt::LogEncryptionVersion(encryption_version);

  if (encryption_version == os_crypt::EncryptionPrefixVersion::kNoVersion) {
    return false;
  }

  // Strip off the versioning prefix before decrypting.
  const std::string raw_ciphertext =
      ciphertext.substr(obfuscation_prefix.length());

  std::optional<std::vector<uint8_t>> maybe_plain =
      crypto::aes_cbc::Decrypt(key, kIv, base::as_byte_span(raw_ciphertext));

  if (maybe_plain) {
    base::UmaHistogramBoolean(kMetricDecryptedWithEmptyKey, false);
    plaintext->assign(base::as_string_view(*maybe_plain));
    return true;
  }

  // Decryption failed - try the empty fallback key. See
  // https://crbug.com/40055416.
  maybe_plain = crypto::aes_cbc::Decrypt(kEmptyKey, kIv,
                                         base::as_byte_span(raw_ciphertext));
  if (maybe_plain) {
    VLOG(1) << "Decryption succeeded after retrying with an empty key";
    base::UmaHistogramBoolean(kMetricDecryptedWithEmptyKey, true);
    plaintext->assign(base::as_string_view(*maybe_plain));
    return true;
  }

  VLOG(1) << "Decryption failed";
  base::UmaHistogramBoolean(kMetricDecryptedWithEmptyKey, false);
  return false;
}

void OSCryptImpl::SetConfig(std::unique_ptr<os_crypt::Config> config) {
  CHECK(!v11_key_);
  config_ = std::move(config);
}

bool OSCryptImpl::IsEncryptionAvailable() {
  // IsEncryptionAvailable() actually means "is real encryption backed by the
  // system secret store available", which here means a v11 key is available,
  // as opposed to the hardcoded v10 obfuscation key. Therefore, try deriving a
  // v11 key - if one is already available this function will just return true.
  return DeriveV11Key();
}

void OSCryptImpl::SetRawEncryptionKey(const std::string& raw_key) {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
  // Check if the v11 password is already cached. If it is, then data encrypted
  // with the old password might not be decryptable.
  CHECK(!v11_key_);
  // The config won't be used if this function is being called. Callers should
  // choose between setting a config and setting a raw encryption key.
  CHECK(!config_);

  if (raw_key.empty()) {
    // Empty key means a v11 key is not available on the browser side. To match
    // the browser's behavior, this OSCryptImpl instance also should not try to
    // derive a v11 key.
    try_v11_ = false;
  } else {
    // If the provided key is non-empty, it's a derived key and can be stored
    // directly. Also, set `try_v11_` regardless of its previous state since a
    // v11 key is now available.
    v11_key_.emplace(std::array<uint8_t, kDerivedKeyBytes>());
    base::span(*v11_key_).copy_from(base::as_byte_span(raw_key));
    try_v11_ = true;
  }
}

std::string OSCryptImpl::GetRawEncryptionKey() {
  return DeriveV11Key() ? std::string(base::as_string_view(*v11_key_))
                        : std::string();
}

void OSCryptImpl::ClearCacheForTesting() {
  v11_key_ = std::nullopt;
  try_v11_ = true;
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
  v11_key_ = Pbkdf2(password);
  try_v11_ = true;
}

crypto::SubtlePassKey OSCryptImpl::MakeCryptoPassKey() {
  return crypto::SubtlePassKey{};
}

std::array<uint8_t, OSCryptImpl::kDerivedKeyBytes> OSCryptImpl::Pbkdf2(
    const std::string& key) {
  std::array<uint8_t, OSCryptImpl::kDerivedKeyBytes> result;
  crypto::kdf::DeriveKeyPbkdf2HmacSha1(kParams, base::as_byte_span(key), kSalt,
                                       result, MakeCryptoPassKey());
  return result;
}

bool OSCryptImpl::DeriveV11Key() {
  base::AutoLock auto_lock(OSCryptImpl::GetLock());
  if (!try_v11_) {
    return false;
  }

  if (v11_key_) {
    return true;
  }

  std::unique_ptr<KeyStorageLinux> key_storage;
  if (storage_provider_factory_for_testing_) {
    key_storage = std::move(storage_provider_factory_for_testing_).Run();
  } else {
    if (config_) {
      key_storage = KeyStorageLinux::CreateService(*config_);
      config_.reset();
    }
  }

  if (!key_storage) {
    // No backend available, can't do v11.
    try_v11_ = false;
    return false;
  }

  std::optional<std::string> maybe_key = key_storage->GetKey();
  if (maybe_key) {
    v11_key_ = Pbkdf2(*maybe_key);
  }

  return v11_key_.has_value();
}

// static
base::Lock& OSCryptImpl::GetLock() {
  static base::NoDestructor<base::Lock> os_crypt_lock;
  return *os_crypt_lock;
}
