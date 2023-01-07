// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/wincrypt_shim.h"
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "crypto/aead.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"

namespace {

// Contains base64 random key encrypted with DPAPI.
constexpr char kOsCryptEncryptedKeyPrefName[] = "os_crypt.encrypted_key";

// AEAD key length in bytes.
constexpr size_t kKeyLength = 256 / 8;

// AEAD nonce length in bytes.
constexpr size_t kNonceLength = 96 / 8;

// Version prefix for data encrypted with profile bound key.
constexpr char kEncryptionVersionPrefix[] = "v10";

// Key prefix for a key encrypted with DPAPI.
constexpr char kDPAPIKeyPrefix[] = "DPAPI";

bool EncryptStringWithDPAPI(const std::string& plaintext,
                            std::string* ciphertext) {
  DATA_BLOB input;
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext.data()));
  input.cbData = static_cast<DWORD>(plaintext.length());

  BOOL result = FALSE;
  DATA_BLOB output;
  {
    SCOPED_UMA_HISTOGRAM_TIMER("OSCrypt.Win.Encrypt.Time");
    result =
        CryptProtectData(&input, L"", nullptr, nullptr, nullptr, 0, &output);
  }
  base::UmaHistogramBoolean("OSCrypt.Win.Encrypt.Result", result);
  if (!result) {
    PLOG(ERROR) << "Failed to encrypt";
    return false;
  }

  // this does a copy
  ciphertext->assign(reinterpret_cast<std::string::value_type*>(output.pbData),
                     output.cbData);

  LocalFree(output.pbData);
  return true;
}

bool DecryptStringWithDPAPI(const std::string& ciphertext,
                            std::string* plaintext) {
  DATA_BLOB input;
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(ciphertext.data()));
  input.cbData = static_cast<DWORD>(ciphertext.length());

  BOOL result = FALSE;
  DATA_BLOB output;
  {
    SCOPED_UMA_HISTOGRAM_TIMER("OSCrypt.Win.Decrypt.Time");
    result = CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0,
                                &output);
  }
  base::UmaHistogramBoolean("OSCrypt.Win.Decrypt.Result", result);
  if (!result) {
    PLOG(ERROR) << "Failed to decrypt";
    return false;
  }

  plaintext->assign(reinterpret_cast<char*>(output.pbData), output.cbData);
  LocalFree(output.pbData);
  return true;
}
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
void RegisterLocalPrefs(PrefRegistrySimple* registry) {
  OSCryptImpl::RegisterLocalPrefs(registry);
}
InitResult InitWithExistingKey(PrefService* local_state) {
  return OSCryptImpl::GetInstance()->InitWithExistingKey(local_state);
}
bool Init(PrefService* local_state) {
  return OSCryptImpl::GetInstance()->Init(local_state);
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
void UseMockKeyForTesting(bool use_mock) {
  OSCryptImpl::GetInstance()->UseMockKeyForTesting(use_mock);
}
void SetLegacyEncryptionForTesting(bool legacy) {
  OSCryptImpl::GetInstance()->SetLegacyEncryptionForTesting(legacy);
}
void ResetStateForTesting() {
  OSCryptImpl::GetInstance()->ResetStateForTesting();
}
}  // namespace OSCrypt

OSCryptImpl::OSCryptImpl() = default;
OSCryptImpl::~OSCryptImpl() = default;

OSCryptImpl* OSCryptImpl::GetInstance() {
  return base::Singleton<OSCryptImpl,
                         base::LeakySingletonTraits<OSCryptImpl>>::get();
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
  if (use_legacy_)
    return EncryptStringWithDPAPI(plaintext, ciphertext);

  crypto::Aead aead(crypto::Aead::AES_256_GCM);

  const auto key = GetRawEncryptionKey();
  aead.Init(&key);

  // Note: can only check these once AEAD is initialized.
  DCHECK_EQ(kKeyLength, aead.KeyLength());
  DCHECK_EQ(kNonceLength, aead.NonceLength());

  std::string nonce;
  crypto::RandBytes(base::WriteInto(&nonce, kNonceLength + 1), kNonceLength);

  if (!aead.Seal(plaintext, nonce, std::string(), ciphertext))
    return false;

  ciphertext->insert(0, nonce);
  ciphertext->insert(0, kEncryptionVersionPrefix);
  return true;
}

bool OSCryptImpl::DecryptString(const std::string& ciphertext,
                            std::string* plaintext) {
  if (!base::StartsWith(ciphertext, kEncryptionVersionPrefix,
                        base::CompareCase::SENSITIVE))
    return DecryptStringWithDPAPI(ciphertext, plaintext);

  crypto::Aead aead(crypto::Aead::AES_256_GCM);

  const auto key = GetRawEncryptionKey();
  aead.Init(&key);

  // Obtain the nonce.
  const std::string nonce =
      ciphertext.substr(sizeof(kEncryptionVersionPrefix) - 1, kNonceLength);
  // Strip off the versioning prefix before decrypting.
  const std::string raw_ciphertext =
      ciphertext.substr(kNonceLength + (sizeof(kEncryptionVersionPrefix) - 1));

  return aead.Open(raw_ciphertext, nonce, std::string(), plaintext);
}

// static
void OSCryptImpl::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kOsCryptEncryptedKeyPrefName, "");
}

bool OSCryptImpl::Init(PrefService* local_state) {
  // Try to pull the key from the local state.
  switch (InitWithExistingKey(local_state)) {
    case OSCrypt::kSuccess:
      return true;
    case OSCrypt::kKeyDoesNotExist:
      break;
    case OSCrypt::kInvalidKeyFormat:
      return false;
    case OSCrypt::kDecryptionFailed:
      break;
  }

  // If there is no key in the local state, or if DPAPI decryption fails,
  // generate a new key.
  std::string key;
  crypto::RandBytes(base::WriteInto(&key, kKeyLength + 1), kKeyLength);

  std::string encrypted_key;
  if (!EncryptStringWithDPAPI(key, &encrypted_key))
    return false;

  // Add header indicating this key is encrypted with DPAPI.
  encrypted_key.insert(0, kDPAPIKeyPrefix);
  std::string base64_key;
  base::Base64Encode(encrypted_key, &base64_key);
  local_state->SetString(kOsCryptEncryptedKeyPrefName, base64_key);
  encryption_key_.assign(key);
  return true;
}

OSCrypt::InitResult OSCryptImpl::InitWithExistingKey(PrefService* local_state) {
  DCHECK(encryption_key_.empty()) << "Key already exists.";
  // Try and pull the key from the local state.
  if (!local_state->HasPrefPath(kOsCryptEncryptedKeyPrefName))
    return OSCrypt::kKeyDoesNotExist;

  const std::string base64_encrypted_key =
      local_state->GetString(kOsCryptEncryptedKeyPrefName);
  std::string encrypted_key_with_header;

  base::Base64Decode(base64_encrypted_key, &encrypted_key_with_header);

  if (!base::StartsWith(encrypted_key_with_header, kDPAPIKeyPrefix,
                        base::CompareCase::SENSITIVE)) {
    NOTREACHED() << "Invalid key format.";
    return OSCrypt::kInvalidKeyFormat;
  }

  const std::string encrypted_key =
      encrypted_key_with_header.substr(sizeof(kDPAPIKeyPrefix) - 1);
  std::string key;
  // This DPAPI decryption can fail if the user's password has been reset
  // by an Administrator.
  if (!DecryptStringWithDPAPI(encrypted_key, &key)) {
    base::UmaHistogramSparse("OSCrypt.Win.KeyDecryptionError",
                             ::GetLastError());
    return OSCrypt::kDecryptionFailed;
  }

  encryption_key_.assign(key);
  return OSCrypt::kSuccess;
}

void OSCryptImpl::SetRawEncryptionKey(const std::string& raw_key) {
  DCHECK(!use_mock_key_) << "Mock key in use.";
  DCHECK(!raw_key.empty()) << "Bad key.";
  DCHECK(encryption_key_.empty()) << "Key already set.";
  encryption_key_.assign(raw_key);
}

std::string OSCryptImpl::GetRawEncryptionKey() {
  if (use_mock_key_) {
    if (mock_encryption_key_.empty())
      mock_encryption_key_.assign(
          crypto::HkdfSha256("peanuts", "salt", "info", kKeyLength));
    DCHECK(!mock_encryption_key_.empty()) << "Failed to initialize mock key.";
    return mock_encryption_key_;
  }

  DCHECK(!encryption_key_.empty()) << "No key.";
  return encryption_key_;
}

bool OSCryptImpl::IsEncryptionAvailable() {
  return !encryption_key_.empty();
}

void OSCryptImpl::UseMockKeyForTesting(bool use_mock) {
  use_mock_key_ = use_mock;
}

void OSCryptImpl::SetLegacyEncryptionForTesting(bool legacy) {
  use_legacy_ = legacy;
}

void OSCryptImpl::ResetStateForTesting() {
  use_legacy_ = false;
  use_mock_key_ = false;
  encryption_key_.clear();
  mock_encryption_key_.clear();
}
