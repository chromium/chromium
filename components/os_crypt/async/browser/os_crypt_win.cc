// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/os_crypt_win.h"

#include <windows.h>

#include <wincrypt.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "crypto/random.h"

namespace os_crypt_async {

namespace {

// Contains base64 random key encrypted with DPAPI.
constexpr char kOsCryptEncryptedKeyPrefName[] = "os_crypt.encrypted_key";

// Whether or not an attempt has been made to enable audit for the DPAPI
// encryption backing the random key.
constexpr char kOsCryptAuditEnabledPrefName[] = "os_crypt.audit_enabled";

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
  result = ::CryptProtectData(
      /*pDataIn=*/&input,
      /*szDataDescr=*/
      base::SysUTF8ToWide(
          base::StrCat(
              {version_info::GetProductName(),
               version_info::IsOfficialBuild() ? "" : " (Developer Build)"}))
          .c_str(),
      /*pOptionalEntropy=*/nullptr,
      /*pvReserved=*/nullptr,
      /*pPromptStruct=*/nullptr, /*dwFlags=*/CRYPTPROTECT_AUDIT,
      /*pDataOut=*/&output);

  if (!result) {
    PLOG(ERROR) << "Failed to encrypt";
    return false;
  }

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
  input.cbData = static_cast<DWORD>(ciphertext.size());

  BOOL result = FALSE;
  DATA_BLOB output;
  result = ::CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0,
                                &output);

  if (!result) {
    return false;
  }

  plaintext->assign(reinterpret_cast<char*>(output.pbData), output.cbData);
  LocalFree(output.pbData);
  return true;
}

bool EncryptAndStoreKey(const std::string& key, PrefService* local_state) {
  std::string encrypted_key;
  if (!EncryptStringWithDPAPI(key, &encrypted_key)) {
    return false;
  }

  encrypted_key.insert(0, kDPAPIKeyPrefix);
  std::string base64_key = base::Base64Encode(encrypted_key);
  local_state->SetString(kOsCryptEncryptedKeyPrefName, base64_key);
  return true;
}

}  // namespace

bool Init(PrefService* local_state) {
  switch (InitWithExistingKey(local_state)) {
    case InitResult::kSuccess:
      return true;
    case InitResult::kKeyDoesNotExist:
    case InitResult::kDecryptionFailed:
      break;
    case InitResult::kInvalidKeyFormat:
      return false;
  }

  std::string key(Encryptor::Key::kAES256GCMKeySize, '\0');
  crypto::RandBytes(base::as_writable_byte_span(key));

  if (!EncryptAndStoreKey(key, local_state)) {
    return false;
  }

  local_state->SetBoolean(kOsCryptAuditEnabledPrefName, true);

  return true;
}

InitResult InitWithExistingKey(PrefService* local_state) {
  if (!local_state->HasPrefPath(kOsCryptEncryptedKeyPrefName)) {
    return InitResult::kKeyDoesNotExist;
  }

  const std::string base64_encrypted_key =
      local_state->GetString(kOsCryptEncryptedKeyPrefName);
  std::string encrypted_key_with_header;

  if (!base::Base64Decode(base64_encrypted_key, &encrypted_key_with_header) ||
      !base::StartsWith(encrypted_key_with_header, kDPAPIKeyPrefix,
                        base::CompareCase::SENSITIVE)) {
    return InitResult::kInvalidKeyFormat;
  }

  const std::string encrypted_key =
      encrypted_key_with_header.substr(sizeof(kDPAPIKeyPrefix) - 1);
  std::string key;
  if (!DecryptStringWithDPAPI(encrypted_key, &key)) {
    base::UmaHistogramSparse("OSCrypt.Win.KeyDecryptionError",
                             ::GetLastError());
    return InitResult::kDecryptionFailed;
  }

  if (!local_state->GetBoolean(kOsCryptAuditEnabledPrefName)) {
    std::ignore = EncryptAndStoreKey(key, local_state);
    local_state->SetBoolean(kOsCryptAuditEnabledPrefName, true);
  }

  return InitResult::kSuccess;
}

void RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kOsCryptEncryptedKeyPrefName, "");
  registry->RegisterBooleanPref(kOsCryptAuditEnabledPrefName, false);
}

}  // namespace os_crypt_async
