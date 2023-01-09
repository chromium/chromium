// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/elevator.h"

#include <dpapi.h>
#include <oleauto.h>

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/win_util.h"
#include "chrome/elevation_service/caller_validation.h"
#include "chrome/elevation_service/elevated_recovery_impl.h"

namespace elevation_service {

namespace {

// Returns a base::Process of the process making the RPC call to us, or invalid
// base::Process if could not be determined.
base::Process GetCallingProcess() {
  // Validation should always be done impersonating the caller.
  HANDLE calling_process_handle;
  RPC_STATUS status = I_RpcOpenClientProcess(
      nullptr, PROCESS_QUERY_LIMITED_INFORMATION, &calling_process_handle);
  // RPC_S_NO_CALL_ACTIVE indicates that the caller is local process.
  if (status == RPC_S_NO_CALL_ACTIVE)
    return base::Process::Current();

  if (status != RPC_S_OK)
    return base::Process();

  return base::Process(calling_process_handle);
}

}  // namespace

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
  if (protection_level > ProtectionLevel::PATH_VALIDATION)
    return E_INVALIDARG;

  UINT length = ::SysStringByteLen(plaintext);

  if (!length)
    return E_INVALIDARG;

  HRESULT hr = ::CoImpersonateClient();
  if (FAILED(hr))
    return hr;

  DATA_BLOB intermediate = {};
  {
    base::ScopedClosureRunner revert_to_self(
        base::BindOnce([]() { ::CoRevertToSelf(); }));

    const auto calling_process = GetCallingProcess();
    if (!calling_process.IsValid())
      return kErrorCouldNotObtainCallingProcess;

    const std::string validation_data =
        GenerateValidationData(protection_level, calling_process);
    if (validation_data.empty())
      return kErrorCouldNotGenerateValidationData;

    std::string data_to_encrypt;
    AppendStringWithLength(validation_data, data_to_encrypt);
    AppendStringWithLength(
        std::string(reinterpret_cast<char*>(plaintext), length),
        data_to_encrypt);

    DATA_BLOB input = {};
    input.cbData = base::checked_cast<DWORD>(data_to_encrypt.length());
    input.pbData = const_cast<BYTE*>(
        reinterpret_cast<const BYTE*>(data_to_encrypt.data()));

    if (!::CryptProtectData(&input, L"", nullptr, nullptr, nullptr, 0,
                            &intermediate)) {
      *last_error = ::GetLastError();
      return kErrorCouldNotEncryptWithUserContext;
    }
  }
  DATA_BLOB output = {};
  {
    base::win::ScopedLocalAlloc intermediate_freer(intermediate.pbData);

    if (!::CryptProtectData(&intermediate, L"", nullptr, nullptr, nullptr, 0,
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

  HRESULT hr = ::CoImpersonateClient();

  if (FAILED(hr))
    return hr;
  std::string plaintext_str;
  {
    DATA_BLOB output = {};
    base::ScopedClosureRunner revert_to_self(
        base::BindOnce([]() { ::CoRevertToSelf(); }));
    // Decrypt using the user store.
    if (!::CryptUnprotectData(&intermediate, nullptr, nullptr, nullptr, nullptr,
                              0, &output)) {
      *last_error = ::GetLastError();
      return kErrorCouldNotDecryptWithUserContext;
    }
    base::win::ScopedLocalAlloc output_freer(output.pbData);

    std::string mutable_plaintext(reinterpret_cast<char*>(output.pbData),
                                  output.cbData);

    std::string validation_data = PopFromStringFront(mutable_plaintext);
    if (validation_data.empty())
      return E_INVALIDARG;
    const auto process = GetCallingProcess();
    if (!process.IsValid()) {
      *last_error = ::GetLastError();
      return kErrorCouldNotObtainCallingProcess;
    }

    // Validation should always be done as the caller.
    bool validated = ValidateData(process, validation_data);
    if (!validated) {
      *last_error = ::GetLastError();
      return kValidationDidNotPass;
    }
    plaintext_str = PopFromStringFront(mutable_plaintext);
  }

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
