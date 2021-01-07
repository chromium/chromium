// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
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
const char kOsCryptEncryptedKeyPrefName[] = "os_crypt.encrypted_key";

// AEAD key length in bytes.
const size_t kKeyLength = 256 / 8;

// AEAD nonce length in bytes.
const size_t kNonceLength = 96 / 8;

// Version prefix for data encrypted with profile bound key.
const char kEncryptionVersionPrefix[] = "v10";

// Key prefix for a key encrypted with DPAPI.
const char kDPAPIKeyPrefix[] = "DPAPI";

// Use mock key instead of a real encryption key. Used for testing.
bool g_use_mock_key = false;

// Store data using the legacy (DPAPI) method rather than session key.
bool g_use_legacy = false;

// These two keys must have no destructors to allow OSCrypt calls to function
// correctly during shutdown.

// Encryption Key. Set either by calling Init() or SetRawEncryptionKey().
std::string& GetEncryptionKeyFactory() {
  static base::NoDestructor<std::string> encryption_key;
  return *encryption_key;
}

// Mock Encryption Key. Only set and used if g_use_mock_key is true.
std::string& GetMockEncryptionKeyFactory() {
  static base::NoDestructor<std::string> mock_encryption_key;
  return *mock_encryption_key;
}

bool EncryptStringWithDPAPI(const std::string& plaintext,
                            std::string* ciphertext) {
  DATA_BLOB input;
  input.pbData =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext.data()));
  input.cbData = static_cast<DWORD>(plaintext.length());

  DATA_BLOB output;
  BOOL result =
      CryptProtectData(&input, L"", nullptr, nullptr, nullptr, 0, &output);
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

  DATA_BLOB output;
  BOOL result = CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                                   0, &output);
  if (!result) {
    PLOG(ERROR) << "Failed to decrypt";
    return false;
  }

  plaintext->assign(reinterpret_cast<char*>(output.pbData), output.cbData);
  LocalFree(output.pbData);
  return true;
}

const std::string& GetEncryptionKeyInternal() {
  if (g_use_mock_key) {
    if (GetMockEncryptionKeyFactory().empty())
      GetMockEncryptionKeyFactory().assign(
          crypto::HkdfSha256("peanuts", "salt", "info", kKeyLength));
    DCHECK(!GetMockEncryptionKeyFactory().empty())
        << "Failed to initialize mock key.";
    return GetMockEncryptionKeyFactory();
  }

  DCHECK(!GetEncryptionKeyFactory().empty()) << "No key.";
  return GetEncryptionKeyFactory();
}

}  // namespace

// static
bool OSCrypt::EncryptString16(const base::string16& plaintext,
                              std::string* ciphertext) {
  return EncryptString(base::UTF16ToUTF8(plaintext), ciphertext);
}

// static
bool OSCrypt::DecryptString16(const std::string& ciphertext,
                              base::string16* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}

// static
bool OSCrypt::EncryptString(const std::string& plaintext,
                            std::string* ciphertext) {
  if (g_use_legacy)
    return EncryptStringWithDPAPI(plaintext, ciphertext);

  crypto::Aead aead(crypto::Aead::AES_256_GCM);

  auto key = GetEncryptionKeyInternal();
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

// static
bool OSCrypt::DecryptString(const std::string& ciphertext,
                            std::string* plaintext) {
  if (!base::StartsWith(ciphertext, kEncryptionVersionPrefix,
                        base::CompareCase::SENSITIVE))
    return DecryptStringWithDPAPI(ciphertext, plaintext);

  crypto::Aead aead(crypto::Aead::AES_256_GCM);

  auto key = GetEncryptionKeyInternal();
  aead.Init(&key);

  // Obtain the nonce.
  std::string nonce =
      ciphertext.substr(sizeof(kEncryptionVersionPrefix) - 1, kNonceLength);
  // Strip off the versioning prefix before decrypting.
  std::string raw_ciphertext =
      ciphertext.substr(kNonceLength + (sizeof(kEncryptionVersionPrefix) - 1));

  return aead.Open(raw_ciphertext, nonce, std::string(), plaintext);
}

// static
void OSCrypt::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kOsCryptEncryptedKeyPrefName, "");
}

// static
bool OSCrypt::Init(PrefService* local_state) {
  DCHECK(GetEncryptionKeyFactory().empty()) << "Key already exists.";
  // Try and pull the key from local state.
  if (local_state->HasPrefPath(kOsCryptEncryptedKeyPrefName)) {
    std::string base64_encrypted_key =
        local_state->GetString(kOsCryptEncryptedKeyPrefName);
    std::string encrypted_key_with_header;

    base::Base64Decode(base64_encrypted_key, &encrypted_key_with_header);

    if (!base::StartsWith(encrypted_key_with_header, kDPAPIKeyPrefix,
                          base::CompareCase::SENSITIVE)) {
      NOTREACHED() << "Invalid key format.";
      return false;
    }
    std::string encrypted_key =
        encrypted_key_with_header.substr(sizeof(kDPAPIKeyPrefix) - 1);
    std::string key;
    // This DPAPI decryption can fail if the user's password has been reset
    // by an Administrator.
    if (DecryptStringWithDPAPI(encrypted_key, &key)) {
      GetEncryptionKeyFactory().assign(key);
      return true;
    }
    base::UmaHistogramSparse("OSCrypt.Win.KeyDecryptionError",
                             ::GetLastError());
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
  GetEncryptionKeyFactory().assign(key);
  return true;
}

// static
void OSCrypt::SetRawEncryptionKey(const std::string& raw_key) {
  DCHECK(!g_use_mock_key) << "Mock key in use.";
  DCHECK(!raw_key.empty()) << "Bad key.";
  DCHECK(GetEncryptionKeyFactory().empty()) << "Key already set.";
  GetEncryptionKeyFactory().assign(raw_key);
}

// static
std::string OSCrypt::GetRawEncryptionKey() {
  return GetEncryptionKeyInternal();
}

// static
bool OSCrypt::IsEncryptionAvailable() {
  return !GetEncryptionKeyFactory().empty();
}

// static
void OSCrypt::UseMockKeyForTesting(bool use_mock) {
  g_use_mock_key = use_mock;
}

// static
void OSCrypt::SetLegacyEncryptionForTesting(bool legacy) {
  g_use_legacy = legacy;
}

// static
void OSCrypt::ResetStateForTesting() {
  g_use_legacy = false;
  g_use_mock_key = false;
  GetEncryptionKeyFactory().clear();
  GetMockEncryptionKeyFactory().clear();
}
