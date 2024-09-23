// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/win_platform_delegate.h"

// clang-format off
#include <windows.h> // Must be in front of other Windows header files.
// clang-format on

#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "crypto/scoped_capi_types.h"
#include "crypto/sha2.h"
#include "net/cert/asn1_util.h"

namespace device_signals {

namespace {

// Returns the SHA-256 hash for the DER-encoded SPKI and subject from the first
// signer cert chain's leaf cert. Return std::nullopt if unable to get to that
// certificate.
std::pair<std::optional<std::string>, std::optional<std::string>> GetSPKIHash(
    HANDLE verify_trust_state_data) {
  std::pair<std::optional<std::string>, std::optional<std::string>> ret;

  CRYPT_PROVIDER_DATA* crypt_provider_data =
      WTHelperProvDataFromStateData(verify_trust_state_data);
  if (!crypt_provider_data) {
    return ret;
  }

  CRYPT_PROVIDER_SGNR* provider_sgnr =
      WTHelperGetProvSignerFromChain(crypt_provider_data,
                                     /*idxSigner=*/0,
                                     /*fCounterSigner=*/false,
                                     /*idxCounterSigner=*/0);
  if (!provider_sgnr) {
    return ret;
  }

  CRYPT_PROVIDER_CERT* provider_cert =
      WTHelperGetProvCertFromChain(provider_sgnr, /*idcCert=*/0);

  if (!provider_cert || !provider_cert->pChainElement) {
    return ret;
  }

  const CERT_CHAIN_ELEMENT* element = provider_cert->pChainElement;
  const CERT_CONTEXT* cert_context = element->pCertContext;
  if (!cert_context) {
    return ret;
  }

  // Get the hash and subject.
  if (cert_context->pbCertEncoded) {
    std::string_view der_bytes(
        reinterpret_cast<const char*>(cert_context->pbCertEncoded),
        cert_context->cbCertEncoded);

    std::string_view spki;
    if (net::asn1::ExtractSPKIFromDERCert(der_bytes, &spki)) {
      ret.first = crypto::SHA256HashString(spki);
    }

    // Get the subject. First ask how long the name is, including null
    // terminator.
    size_t length = CertGetNameStringA(
        cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, /*dwFlags=*/0,
        /*pvTypePara=*/nullptr, /*pszNameString=*/nullptr, /*cchNameString=*/0);
    if (length > 0) {
      std::vector<char> subject(length);
      CertGetNameStringA(
          cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, /*dwFlags=*/0,
          /*pvTypePara=*/nullptr, /*pszNameString=*/subject.data(),
          /*cchNameString=*/subject.size());
      ret.second = subject.data();
    }
  }

  return ret;
}

}  // namespace

WinPlatformDelegate::WinPlatformDelegate() = default;

WinPlatformDelegate::~WinPlatformDelegate() = default;

bool WinPlatformDelegate::ResolveFilePath(const base::FilePath& file_path,
                                          base::FilePath* resolved_file_path) {
  return ResolvePath(file_path, resolved_file_path);
}

std::optional<PlatformDelegate::SigningCertificatesPublicKeys>
WinPlatformDelegate::GetSigningCertificatesPublicKeys(
    const base::FilePath& file_path) {
  SigningCertificatesPublicKeys public_keys;

  WINTRUST_FILE_INFO file_info{};
  file_info.cbStruct = sizeof(file_info);
  file_info.pcwszFilePath = file_path.value().c_str();
  file_info.hFile = NULL;
  file_info.pgKnownSubject = NULL;

  WINTRUST_DATA wintrust_data{};
  wintrust_data.cbStruct = sizeof(wintrust_data);
  wintrust_data.pPolicyCallbackData = NULL;
  wintrust_data.pSIPClientData = NULL;
  wintrust_data.dwUIChoice = WTD_UI_NONE;
  wintrust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
  wintrust_data.dwUnionChoice = WTD_CHOICE_FILE;
  wintrust_data.pFile = &file_info;
  wintrust_data.dwStateAction = WTD_STATEACTION_VERIFY;
  wintrust_data.hWVTStateData = NULL;
  wintrust_data.pwszURLReference = NULL;
  // Disallow revocation checks over the network.
  wintrust_data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

  // Check first signature first while getting the secondary signatures'
  // count, iterate over those after.
  WINTRUST_SIGNATURE_SETTINGS signature_settings{};
  signature_settings.cbStruct = sizeof(signature_settings);
  signature_settings.dwIndex = 0;
  signature_settings.dwFlags =
      WSS_VERIFY_SPECIFIC | WSS_GET_SECONDARY_SIG_COUNT;

  wintrust_data.pSignatureSettings = &signature_settings;

  GUID policy_guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  LONG trust = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),
                              &policy_guid, &wintrust_data);
  auto primary_hash_subject = GetSPKIHash(wintrust_data.hWVTStateData);
  if (!primary_hash_subject.first) {
    // No values could be extracted for the primary signature, return early.
    return public_keys;
  }

  public_keys.is_os_verified = trust == 0;
  public_keys.hashes.push_back(primary_hash_subject.first.value());

  if (primary_hash_subject.second) {
    public_keys.subject_name = primary_hash_subject.second.value();
  }

  // Collect SPKI hashes for secondary signatures' certs.
  DWORD secondary_signatures_count =
      wintrust_data.pSignatureSettings->cSecondarySigs;
  wintrust_data.pSignatureSettings->dwFlags = WSS_VERIFY_SPECIFIC;
  for (DWORD i = 0; i < secondary_signatures_count; ++i) {
    // Free the previous provider data.
    wintrust_data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &policy_guid,
                   &wintrust_data);
    wintrust_data.hWVTStateData = NULL;

    // Secondary signatures start at index 1 as 0 is the primary signature.
    wintrust_data.dwStateAction = WTD_STATEACTION_VERIFY;
    wintrust_data.pSignatureSettings->dwIndex = i + 1;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &policy_guid,
                   &wintrust_data);

    auto secondary_hash_subject = GetSPKIHash(wintrust_data.hWVTStateData);
    if (secondary_hash_subject.first) {
      public_keys.hashes.push_back(secondary_hash_subject.first.value());
    }
  }

  // Free the previous provider data.
  wintrust_data.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &policy_guid,
                 &wintrust_data);

  return public_keys;
}

}  // namespace device_signals
