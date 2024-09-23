// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt.h"

#include <windows.h>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/wincrypt_shim.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "crypto/aead.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"

namespace {

// Contains base64 random key encrypted with DPAPI.
constexpr char kOsCryptEncryptedKeyPrefName[] = "os_crypt.encrypted_key";

// Whether or not an attempt has been made to enable audit for the DPAPI
// encryption backing the random key.
constexpr char kOsCryptAuditEnabledPrefName[] = "os_crypt.audit_enabled";

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
    result = ::CryptProtectData(
        /*pDataIn=*/&input,
        /*szDataDescr=*/
        base::SysUTF8ToWide(version_info::GetProductName()).c_str(),
        /*pOptionalEntropy=*/nullptr,
        /*pvReserved=*/nullptr,
        /*pPromptStruct=*/nullptr, /*dwFlags=*/CRYPTPROTECT_AUDIT,
        /*pDataOut=*/&output);
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

// Takes `key` and encrypts it with DPAPI, then stores it in the `local_state`.
// Returns true if the key was successfully encrypted and stored.
bool EncryptAndStoreKey(const std::string& key, PrefService* local_state) {
  std::string encrypted_key;
  if (!EncryptStringWithDPAPI(key, &encrypted_key)) {
    return false;
  }

  // Add header indicating this key is encrypted with DPAPI.
  encrypted_key.insert(0, kDPAPIKeyPrefix);
  std::string base64_key = base::Base64Encode(encrypted_key);
  local_state->SetString(kOsCryptEncryptedKeyPrefName, base64_key);
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

  std::string nonce(kNonceLength, '\0');
  crypto::RandBytes(base::as_writable_byte_span(nonce));

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
  registry->RegisterBooleanPref(kOsCryptAuditEnabledPrefName, false);
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
  std::string key(kKeyLength, '\0');
  crypto::RandBytes(base::as_writable_byte_span(key));

  if (!EncryptAndStoreKey(key, local_state)) {
    return false;
  }

  // This new key is already encrypted with audit flag enabled.
  local_state->SetBoolean(kOsCryptAuditEnabledPrefName, true);

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

  if (!local_state->GetBoolean(kOsCryptAuditEnabledPrefName)) {
    // In theory, EncryptAndStoreKey could fail if DPAPI fails to encrypt, but
    // DPAPI decrypted the old data fine. In this case it's better to leave the
    // previously encrypted key, since the code has been able to decrypt it.
    // Trying over and over makes no sense so the code explicitly does not
    // attempt again, and audit will simply not be enabled in this case.
    std::ignore = EncryptAndStoreKey(key, local_state);

    // Indicate that an attempt has been made to turn audit flag on, so retry is
    // not attempted.
    local_state->SetBoolean(kOsCryptAuditEnabledPrefName, true);
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
  if (use_mock_key_) {
    return !GetRawEncryptionKey().empty();
  }
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
