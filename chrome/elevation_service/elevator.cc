// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/elevator.h"

#include <dpapi.h>
#include <oleauto.h>
#include <stdint.h>
#include <userenv.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version_info/version_info.h"
#include "base/win/access_token.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/scoped_process_information.h"
#include "base/win/startup_information.h"
#include "base/win/windows_handle_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/elevation_service/caller_validation.h"
#include "chrome/elevation_service/elevated_recovery_impl.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/windows_services/service_program/get_calling_process.h"
#include "chrome/windows_services/service_program/scoped_client_impersonation.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/elevation_service/internal/elevation_service_internal.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace elevation_service {

namespace {

ProtectionLevel RemoveFlags(ProtectionLevel protection_level,
                            EncryptFlags& flags) {
  // Flag value extraction goes here. Currently no flags are supported.
  return static_cast<ProtectionLevel>(
      internal::ExtractProtectionLevel(protection_level));
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
  EncryptFlags flags;
  protection_level = RemoveFlags(protection_level, flags);

  if (protection_level >= ProtectionLevel::PROTECTION_MAX) {
    return kErrorUnsupportedProtectionLevel;
  }

  UINT length = ::SysStringByteLen(plaintext);

  if (!length)
    return E_INVALIDARG;

  std::string plaintext_str(reinterpret_cast<char*>(plaintext), length);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  InternalFlags pre_process_flags;
  auto pre_process_result = PreProcessData(plaintext_str, &pre_process_flags);
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
            base::SysUTF8ToWide(base::StrCat({version_info::GetProductName(),
                                              version_info::IsOfficialBuild()
                                                  ? ""
                                                  : " (Developer Build)"}))
                .c_str(),
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
      return kErrorInvalidValidationData;
    }
    const auto data =
        std::vector<uint8_t>(validation_data.cbegin(), validation_data.cend());
    const auto process = GetCallingProcess();
    if (!process.IsValid()) {
      *last_error = ::GetLastError();
      return kErrorCouldNotObtainCallingProcess;
    }

    // Note: Validation should always be done using caller impersonation token.
    HRESULT validation_result = ValidateData(process, data);

    if (FAILED(validation_result)) {
      *last_error = ::GetLastError();
      return validation_result;
    }
    plaintext_str = PopFromStringFront(mutable_plaintext);
  } else {
    return impersonate.result();
  }
  bool should_reencrypt = false;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  InternalFlags flags;
  auto post_process_result = PostProcessData(plaintext_str, &flags);
  if (!post_process_result.has_value()) {
    return post_process_result.error();
  }
  plaintext_str.swap(*post_process_result);
  if (flags.post_process_should_reencrypt) {
    should_reencrypt = true;
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  *plaintext =
      ::SysAllocStringByteLen(plaintext_str.c_str(), plaintext_str.length());

  if (!*plaintext)
    return E_OUTOFMEMORY;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFakeReencryptForTestingSwitch)) {
    should_reencrypt = true;
  }
  return should_reencrypt ? kSuccessShouldReencrypt : S_OK;
}

HRESULT Elevator::RunIsolatedChrome(DWORD flags,
                                    const WCHAR* command_line,
                                    [[maybe_unused]] BSTR* log,
                                    ULONG_PTR* proc_handle,
                                    DWORD* last_error) {
  *last_error = ERROR_SUCCESS;
  bool success = false;
  absl::Cleanup maybe_set_last_error = [last_error, &success]() {
    if (!success) {
      *last_error = ::GetLastError();
    }
  };

  std::optional<base::win::AccessToken> impersonation_token;

  if (ScopedClientImpersonation impersonate; impersonate.is_valid()) {
    impersonation_token = base::win::AccessToken::FromCurrentThread(
        /*open_as_self=*/true, TOKEN_DUPLICATE | TOKEN_QUERY);
  } else {
    return impersonate.result();
  }

  if (!impersonation_token) {
    PLOG(ERROR) << "Cannot create impersonation token.";
    return kErrorCouldNotObtainThreadToken;
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto primary_token = CreatePrimaryToken(std::move(*impersonation_token));
#else
  auto primary_token = impersonation_token->DuplicatePrimary(
      TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  base::win::StartupInformation startup_information;
  if (!startup_information.InitializeProcThreadAttributeList(
          /*attribute_count=*/1u)) {
    PLOG(ERROR) << "Cannot Init process and thread attribute list.";
    return E_OUTOFMEMORY;
  }

  base::Process calling_process =
      GetCallingProcess(PROCESS_CREATE_PROCESS | PROCESS_DUP_HANDLE);
  if (!calling_process.IsValid()) {
    PLOG(ERROR) << "Could not obtain calling process.";
    return kErrorCouldNotObtainCallingProcess;
  }

  HANDLE parent = calling_process.Handle();

  startup_information.UpdateProcThreadAttribute(
      PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &parent, sizeof(parent));

  base::CommandLine untrusted_command_line =
      base::CommandLine::FromString(command_line);
  std::optional<base::CommandLine> trusted_command_line;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowUntrustedPathForTesting)) {
    trusted_command_line.emplace(untrusted_command_line.GetProgram());
  } else {
    base::FilePath chrome_exe;
    if (!base::PathService::Get(base::DIR_EXE, &chrome_exe)) {
      return kErrorChromePathNotFound;
    }
    chrome_exe = chrome_exe.DirName().Append(installer::kChromeExe);
    if (!base::PathExists(chrome_exe)) {
      LOG(ERROR) << "Chrome path does not exist at " << chrome_exe;
      return kErrorChromePathNotFound;
    }

    trusted_command_line.emplace(chrome_exe);
  }
  const auto other_args = untrusted_command_line.GetArgs();

  const char* const kAllowedSwitches[] = {
      // Allow selection of profile.
      ::switches::kProfileDirectory, ::switches::kUserDataDir};

  trusted_command_line->CopySwitchesFrom(untrusted_command_line,
                                         kAllowedSwitches);

  trusted_command_line->AppendSwitch(::switches::kIsolated);
  for (const auto& arg : other_args) {
    // Safety check.
    if (arg[0] == L'-') {
      continue;
    }
    trusted_command_line->AppendArgNative(arg);
  }

  std::wstring writeable_command_line(
      trusted_command_line->GetCommandLineString());

  DWORD creation_flags = CREATE_UNICODE_ENVIRONMENT;
  if (startup_information.has_extended_startup_info()) {
    creation_flags |= EXTENDED_STARTUPINFO_PRESENT;
  }
  LPVOID env;
  if (!::CreateEnvironmentBlock(&env, primary_token->get(),
                                /*bInherit=*/false)) {
    PLOG(ERROR) << "Cannot create user environment.";
    return kErrorCouldNotObtainUserEnvironment;
  }

  // TODO(wfh): Sanitize environment especially the PATH.
  absl::Cleanup destroy_environment = [&env] {
    ::DestroyEnvironmentBlock(env);
  };

  PROCESS_INFORMATION temp_process_info = {};
  if (!::CreateProcessAsUserW(
          primary_token->get(),
          std::data(trusted_command_line->GetProgram().value()),
          std::data(writeable_command_line),
          /*lpProcessAttributes=*/nullptr, /*lpThreadAttributes=*/nullptr,
          /*bInheritHandles=*/FALSE,
          /*dwCreationFlags=*/creation_flags,
          /*lpEnvironment=*/env, /*lpCurrentDirectory=*/
          std::data(trusted_command_line->GetProgram().DirName().value()),
          startup_information.startup_info(), &temp_process_info)) {
    PLOG(ERROR) << "Cannot create browser process.";
    return kErrorCouldNotLaunchBrowser;
  }
  base::win::ScopedProcessInformation process_info(temp_process_info);

  HANDLE duplicate_proc_handle = nullptr;

  if (!::DuplicateHandle(
          /*hSourceProcessHandle=*/::GetCurrentProcess(),
          /*hSourceHandle=*/process_info.process_handle(),
          /*hTargetProcessHandle=*/calling_process.Handle(),
          /*lpTargetHandle=*/&duplicate_proc_handle,
          /*dwDesiredAccess=*/PROCESS_QUERY_LIMITED_INFORMATION |
              PROCESS_TERMINATE | SYNCHRONIZE,
          /*bInheritHandle=*/FALSE, /*dwOptions=*/0)) {
    PLOG(ERROR) << "Cannot duplicate browser process handle.";
    return kErrorCouldNotDuplicateHandle;
  }

  *proc_handle = base::win::HandleToUint32(duplicate_proc_handle);

  success = true;
  return S_OK;
}

HRESULT Elevator::AcceptInvitation(const wchar_t* server_name) {
  return E_NOTIMPL;
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
  size =
      base::U32FromLittleEndian(base::as_byte_span(str).first<sizeof(size)>());
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
