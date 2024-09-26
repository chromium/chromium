// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/elevator.h"

#include <dpapi.h>
#include <oleauto.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version_info/channel.h"
#include "base/version_info/version_info.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "chrome/elevation_service/caller_validation.h"
#include "chrome/elevation_service/elevated_recovery_impl.h"
#include "chrome/install_static/install_util.h"
#include "chrome/windows_services/service_program/get_calling_process.h"
#include "chrome/windows_services/service_program/scoped_client_impersonation.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/elevation_service/internal/elevation_service_internal.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace elevation_service {

HRESULT Elevator::RunRecoveryCRXElevated(const wchar_t* crx_path,
                                         const wchar_t* browser_appid,
                                         const wchar_t* browser_version,
                                         const wchar_t* session_id,
                                         DWORD caller_proc_id,
                                         ULONG_PTR* proc_handle) {
  base::win::ScopedHandle scoped_proc_handle;
  HRESULT hr = RunChromeRecoveryCRX(base::FilePath(crx_path), browser_appid,
                                    browser_version, session_id, caller_proc_id,
                                    &scoped_proc_handle);
  *proc_handle = base::win::HandleToUint32(scoped_proc_handle.Take());
  return hr;
}

HRESULT Elevator::EncryptData(ProtectionLevel protection_level,
                              const BSTR plaintext,
                              BSTR* ciphertext,
                              DWORD* last_error) {
  if (protection_level >= ProtectionLevel::PROTECTION_MAX) {
    return kErrorUnsupportedProtectionLevel;
  }

  UINT length = ::SysStringByteLen(plaintext);

  if (!length)
    return E_INVALIDARG;

  std::string plaintext_str(reinterpret_cast<char*>(plaintext), length);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto pre_process_result = PreProcessData(plaintext_str);
  if (!pre_process_result.has_value()) {
    return pre_process_result.error();
  }
  plaintext_str.swap(*pre_process_result);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  DATA_BLOB intermediate = {};
  if (ScopedClientImpersonation impersonate; impersonate.is_valid()) {
    const auto calling_process = GetCallingProcess();
    if (!calling_process.IsValid())
      return kErrorCouldNotObtainCallingProcess;

    const auto validation_data =
        GenerateValidationData(protection_level, calling_process);
    if (!validation_data.has_value()) {
      return validation_data.error();
    }
    const auto data =
        std::string(validation_data->cbegin(), validation_data->cend());

    std::string data_to_encrypt;
    AppendStringWithLength(data, data_to_encrypt);
    AppendStringWithLength(plaintext_str, data_to_encrypt);

    DATA_BLOB input = {};
    input.cbData = base::checked_cast<DWORD>(data_to_encrypt.length());
    input.pbData = const_cast<BYTE*>(
        reinterpret_cast<const BYTE*>(data_to_encrypt.data()));

    if (!::CryptProtectData(
            &input, /*szDataDescr=*/
            base::SysUTF8ToWide(version_info::GetProductName()).c_str(),
            nullptr, nullptr, nullptr, /*dwFlags=*/CRYPTPROTECT_AUDIT,
            &intermediate)) {
      *last_error = ::GetLastError();
      return kErrorCouldNotEncryptWithUserContext;
    }
  } else {
    return impersonate.result();
  }
  DATA_BLOB output = {};
  {
    base::win::ScopedLocalAlloc intermediate_freer(intermediate.pbData);

    if (!::CryptProtectData(
            &intermediate,
            /*szDataDescr=*/
            base::SysUTF8ToWide(version_info::GetProductName()).c_str(),
            nullptr, nullptr, nullptr, /*dwFlags=*/CRYPTPROTECT_AUDIT,
            &output)) {
      *last_error = ::GetLastError();
      return kErrorCouldNotEncryptWithSystemContext;
    }
  }
  base::win::ScopedLocalAlloc output_freer(output.pbData);

  *ciphertext = ::SysAllocStringByteLen(reinterpret_cast<LPCSTR>(output.pbData),
                                        output.cbData);

  if (!*ciphertext)
    return E_OUTOFMEMORY;

  return S_OK;
}

HRESULT Elevator::DecryptData(const BSTR ciphertext,
                              BSTR* plaintext,
                              DWORD* last_error) {
  UINT length = ::SysStringByteLen(ciphertext);

  if (!length)
    return E_INVALIDARG;

  DATA_BLOB input = {};
  input.cbData = length;
  input.pbData = reinterpret_cast<BYTE*>(ciphertext);

  DATA_BLOB intermediate = {};

  // Decrypt using the SYSTEM dpapi store.
  if (!::CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0,
                            &intermediate)) {
    *last_error = ::GetLastError();
    return kErrorCouldNotDecryptWithSystemContext;
  }

  base::win::ScopedLocalAlloc intermediate_freer(intermediate.pbData);

  std::string plaintext_str;
  if (ScopedClientImpersonation impersonate; impersonate.is_valid()) {
    DATA_BLOB output = {};
    // Decrypt using the user store.
    if (!::CryptUnprotectData(&intermediate, nullptr, nullptr, nullptr, nullptr,
                              0, &output)) {
      *last_error = ::GetLastError();
      return kErrorCouldNotDecryptWithUserContext;
    }
    base::win::ScopedLocalAlloc output_freer(output.pbData);

    std::string mutable_plaintext(reinterpret_cast<char*>(output.pbData),
                                  output.cbData);

    const std::string validation_data = PopFromStringFront(mutable_plaintext);
    if (validation_data.empty()) {
      return E_INVALIDARG;
    }
    const auto data =
        std::vector<uint8_t>(validation_data.cbegin(), validation_data.cend());
    const auto process = GetCallingProcess();
    if (!process.IsValid()) {
      *last_error = ::GetLastError();
      return kErrorCouldNotObtainCallingProcess;
    }

    // Note: Validation should always be done using caller impersonation token.
    std::string log_message;
    HRESULT validation_result = ValidateData(process, data, &log_message);

    if (FAILED(validation_result)) {
      *last_error = ::GetLastError();
      // Only enable extended logging on Dev channel.
      if (install_static::GetChromeChannel() == version_info::Channel::DEV &&
          !log_message.empty()) {
        *plaintext =
            ::SysAllocStringByteLen(log_message.c_str(), log_message.length());
      }
      return validation_result;
    }
    plaintext_str = PopFromStringFront(mutable_plaintext);
  } else {
    return impersonate.result();
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto post_process_result = PostProcessData(plaintext_str);
  if (!post_process_result.has_value()) {
    return post_process_result.error();
  }
  plaintext_str.swap(*post_process_result);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  *plaintext =
      ::SysAllocStringByteLen(plaintext_str.c_str(), plaintext_str.length());

  if (!*plaintext)
    return E_OUTOFMEMORY;

  return S_OK;
}

// static
void Elevator::AppendStringWithLength(const std::string& to_append,
                                      std::string& base) {
  uint32_t size = base::checked_cast<uint32_t>(to_append.length());
  base.append(reinterpret_cast<char*>(&size), sizeof(size));
  base.append(to_append);
}

// static
std::string Elevator::PopFromStringFront(std::string& str) {
  uint32_t size;
  if (str.length() < sizeof(size))
    return std::string();
  auto it = str.begin();
  // Obtain the size.
  memcpy(&size, str.c_str(), sizeof(size));
  // Skip over the size field.
  std::string value;
  if (size) {
    it += sizeof(size);
    // Pull the string out.
    value.assign(it, it + size);
    DCHECK_EQ(value.length(), base::checked_cast<std::string::size_type>(size));
  }
  // Trim the string to the remainder.
  str = str.substr(sizeof(size) + size);
  return value;
}

}  // namespace elevation_service
