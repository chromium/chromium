// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/os_process_manager.h"

#include <Windows.h>

#include <MDMRegistration.h>
#include <Shellapi.h>  // For CommandLineToArgvW()
#include <Shlobj.h>
#include <Winternl.h>
#include <aclapi.h>
#include <atlconv.h>
#include <dpapi.h>
#include <malloc.h>
#include <memory.h>
#include <sddl.h>
#include <security.h>
#include <stdlib.h>
#include <userenv.h>
#include <wincred.h>

#include <iomanip>
#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/scoped_native_library.h"
#include "base/strings/strcat_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_process_information.h"
#include "base/win/win_util.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/scoped_handle.h"

typedef NTSTATUS(FAR WINAPI* NtOpenDirectoryObjectPfn)(
    OUT PHANDLE DirectoryHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes);

namespace credential_provider {

namespace {

HRESULT GetTokenLogonSID(const base::win::ScopedHandle& token, PSID* sid) {
  LOGFN(VERBOSE);
  DCHECK(sid);

  // TODO: make more robust by asking for needed length first.
  char buffer[256];
  DWORD returned_length;
  if (!::GetTokenInformation(token.Get(), TokenLogonSid, &buffer,
                             std::size(buffer), &returned_length)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetTokenInformation hr=" << putHR(hr);
    return hr;
  }

  // Make sure the data returned is correct.
  TOKEN_GROUPS* groups = reinterpret_cast<TOKEN_GROUPS*>(buffer);
  if (groups->GroupCount != 1) {
    LOGFN(ERROR) << "GetTokenInformation count=" << groups->GroupCount;
    return E_UNEXPECTED;
  }

  if ((groups->Groups[0].Attributes & SE_GROUP_LOGON_ID) != SE_GROUP_LOGON_ID) {
    LOGFN(ERROR) << "GetTokenInformation not a logon sid attr="
                 << std::setbase(16) << groups->Groups[0].Attributes;
    return E_UNEXPECTED;
  }

  if (!::IsValidSid(groups->Groups[0].Sid)) {
    LOGFN(ERROR) << "GetTokenInformation not valid sid attr="
                 << std::setbase(16) << groups->Groups[0].Attributes;
    return E_UNEXPECTED;
  }

  DWORD length = ::GetLengthSid(groups->Groups[0].Sid);
  *sid = reinterpret_cast<SID*>(LocalAlloc(LMEM_FIXED, length));
  if (*sid == nullptr) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "LocalAlloc sid hr=" << putHR(hr);
    return hr;
  }

  ::CopySid(length, *sid, groups->Groups[0].Sid);
  return S_OK;
}

HRESULT AddAllowedACE(ACL* dacl,
                      DWORD flags,
                      DWORD access_mask,
                      PSID sid,
                      ACL** new_dacl) {
  LOGFN(VERBOSE);
  DCHECK(new_dacl);

  ACL_SIZE_INFORMATION si;
  if (!::GetAclInformation(dacl, &si, sizeof(si), AclSizeInformation)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetAclInformation hr=" << putHR(hr);
    return hr;
  }

  // Allocate memory for the existing DACL plus the new entry.
  DWORD new_dacl_size = si.AclBytesInUse + sizeof(ACCESS_ALLOWED_ACE) +
                        ::GetSidLengthRequired(SID_MAX_SUB_AUTHORITIES);
  std::unique_ptr<ACL, decltype(&LocalFree)> local_dacl(
      reinterpret_cast<ACL*>(::LocalAlloc(GPTR, new_dacl_size)), ::LocalFree);
  if (local_dacl == nullptr) {
    HRESULT hr = E_OUTOFMEMORY;
    LOGFN(ERROR) << "LocalAlloc ACL hr=" << putHR(hr);
    return hr;
  }

  if (!::InitializeAcl(local_dacl.get(), new_dacl_size, ACL_REVISION)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "InitializeAcl hr=" << putHR(hr);
    return hr;
  }

  bool inserted = false;
  for (DWORD i = 0; i < si.AceCount; ++i) {
    ACE_HEADER* ace;
    if (!GetAce(dacl, i, reinterpret_cast<void**>(&ace))) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "GetAe i=" << i << " hr=" << putHR(hr);
      return hr;
    }

    if (!inserted && ace->AceFlags & INHERITED_ACE) {
      if (!::AddAccessAllowedAceEx(local_dacl.get(), ACL_REVISION, flags,
                                   access_mask, sid)) {
        HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
        LOGFN(ERROR) << "AddAccessAllowedAceEx i=" << i << " hr=" << putHR(hr);
        return hr;
      }

      inserted = true;
    }

    if (!::AddAce(local_dacl.get(), ACL_REVISION, MAXWORD, ace, ace->AceSize)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "AddAce i=" << i << " hr=" << putHR(hr);
      return hr;
    }
  }

  if (!inserted) {
    if (!::AddAccessAllowedAceEx(local_dacl.get(), ACL_REVISION, flags,
                                 access_mask, sid)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "AddAccessAllowedAceEx hr=" << putHR(hr);
      return hr;
    }
  }

  *new_dacl = local_dacl.release();
  return S_OK;
}

HRESULT AllowLogonSIDOnLocalBasedNamedObjects(PSID sid) {
  HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");  // Don't close handle.
  if (ntdll == nullptr) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetModuleHandleW hr=" << putHR(hr);
    return hr;
  }

  NtOpenDirectoryObjectPfn NtOpenDirectoryObject =
      reinterpret_cast<NtOpenDirectoryObjectPfn>(
          ::GetProcAddress(ntdll, "NtOpenDirectoryObject"));
  if (NtOpenDirectoryObject == nullptr) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetProcAddress hr=" << putHR(hr);
    return hr;
  }

  DWORD session_id;
  if (!::ProcessIdToSessionId(GetCurrentProcessId(), &session_id)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ProcessIdToSessionId hr=" << putHR(hr);
    return hr;
  }

  LOGFN(VERBOSE) << "session=" << session_id;

  UNICODE_STRING name;
  wchar_t name_buffer[64];
  if (session_id == 0) {
    _snwprintf_s(name_buffer, std::size(name_buffer), L"\\BaseNamedObjects");
  } else {
    _snwprintf_s(name_buffer, std::size(name_buffer),
                 L"\\Sessions\\%d\\BaseNamedObjects", session_id);
  }
  InitWindowsStringWithString(name_buffer, &name);

  OBJECT_ATTRIBUTES oa;
  oa.Length = sizeof(oa);
  oa.RootDirectory = nullptr;
  oa.ObjectName = &name;
  oa.Attributes = 0;
  oa.SecurityDescriptor = nullptr;
  oa.SecurityQualityOfService = nullptr;

  const ACCESS_MASK kDesiredAccess =
      DIRECTORY_TRAVERSE | READ_CONTROL | WRITE_DAC;
  base::win::ScopedHandle::Handle handle;
  NTSTATUS sts = (*NtOpenDirectoryObject)(&handle, kDesiredAccess, &oa);
  if (sts != STATUS_SUCCESS) {
    HRESULT hr = HRESULT_FROM_NT(sts);
    LOGFN(ERROR) << "NtOpenDirectoryObject hr=" << putHR(hr);
    return hr;
  }
  base::win::ScopedHandle dir_handle(handle);

  PSECURITY_DESCRIPTOR sd;
  ACL* dacl;  // Not owned.
  DWORD err = ::GetSecurityInfo(dir_handle.Get(), SE_WINDOW_OBJECT,
                                DACL_SECURITY_INFORMATION, nullptr, nullptr,
                                &dacl, nullptr, &sd);
  if (err != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_NT(err);
    LOGFN(ERROR) << "GetSecurityInfo hr=" << putHR(hr);
    return hr;
  }

  const DWORD kDesiredSidAccess = DIRECTORY_QUERY | DIRECTORY_TRAVERSE |
                                  DIRECTORY_CREATE_OBJECT |
                                  DIRECTORY_CREATE_SUBDIRECTORY;
  ACL* new_dacl = nullptr;
  HRESULT hr = AddAllowedACE(dacl, NO_PROPAGATE_INHERIT_ACE, kDesiredSidAccess,
                             sid, &new_dacl);
  ::LocalFree(sd);  // This "frees" dacl too.
  if (FAILED(hr)) {
    LOGFN(ERROR) << "AddAllowedACE 0 hr=" << putHR(hr);
    return hr;
  }

  err = ::SetSecurityInfo(dir_handle.Get(), SE_WINDOW_OBJECT,
                          DACL_SECURITY_INFORMATION, nullptr, nullptr, new_dacl,
                          nullptr);
  ::LocalFree(new_dacl);
  if (err != ERROR_SUCCESS) {
    hr = HRESULT_FROM_NT(err);
    LOGFN(ERROR) << "SetSecurityInfo hr=" << putHR(hr);
    return hr;
  }

  return S_OK;
}

HRESULT AllowLogonSIDOnWinSta0(PSID sid) {
  LOGFN(VERBOSE);

  ScopedWindowStationHandle winsta0(
      ::OpenWindowStationW(L"WinSta0", FALSE, READ_CONTROL | WRITE_DAC));
  if (!winsta0.IsValid()) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "OpenWindowStation hr=" << putHR(hr);
    return hr;
  }

  PSECURITY_DESCRIPTOR sd;
  ACL* dacl;  // Not owned.
  DWORD err = ::GetSecurityInfo(winsta0.Get(), SE_WINDOW_OBJECT,
                                DACL_SECURITY_INFORMATION, nullptr, nullptr,
                                &dacl, nullptr, &sd);
  if (err != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_NT(err);
    LOGFN(ERROR) << "GetSecurityInfo hr=" << putHR(hr);
    return hr;
  }

  // Add DACL entries.  This is the minimum set of access rights needed for
  // a simple MFC app to run.
  const DWORD kDesiredAccess =
      WINSTA_ACCESSGLOBALATOMS | WINSTA_READSCREEN | WINSTA_EXITWINDOWS |
      READ_CONTROL |
      // The below are needed to run Chrome.  In particular,
      // WINSTA_WRITEATTRIBUTES is needed so that keyboard shortcuts works.
      // WINSTA_CREATEDESKTOP is needed in order for Chrome's sandboxing
      // to work.
      WINSTA_CREATEDESKTOP | WINSTA_READATTRIBUTES | WINSTA_WRITEATTRIBUTES;
  ACL* new_dacl = nullptr;
  HRESULT hr = AddAllowedACE(dacl, NO_PROPAGATE_INHERIT_ACE, kDesiredAccess,
                             sid, &new_dacl);
  LocalFree(sd);  // This "frees" dacl too.
  if (FAILED(hr)) {
    LOGFN(ERROR) << "AddAllowedACE 0 hr=" << putHR(hr);
    return hr;
  }

  err = ::SetSecurityInfo(winsta0.Get(), SE_WINDOW_OBJECT,
                          DACL_SECURITY_INFORMATION, nullptr, nullptr, new_dacl,
                          nullptr);
  ::LocalFree(new_dacl);
  if (err != ERROR_SUCCESS) {
    hr = HRESULT_FROM_NT(err);
    LOGFN(ERROR) << "SetSecurityInfo hr=" << putHR(hr);
    return hr;
  }

  // Usually a window station also sets inherit-only permissions for apps that
  // create new desktops.  However, the gaia logon app is not expected to do
  // that (nor is the logon session being given CREATEDESKTOP permission above
  // anyway) so not setting any inherited permissions for the logon session.

  return S_OK;
}

HDESK GetAndAllowLogonSIDOnDesktop(const wchar_t* desktop_name,
                                   PSID sid,
                                   DWORD desired_access) {
  LOGFN(VERBOSE);

  const DWORD kDesiredAccess =
      desired_access | READ_CONTROL | WRITE_DAC | DESKTOP_CREATEWINDOW;
  ScopedDesktopHandle desktop(
      ::OpenDesktop(desktop_name, 0, FALSE, kDesiredAccess));
  if (!desktop.IsValid()) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
      desktop.Set(::CreateDesktop(desktop_name, nullptr, nullptr, 0,
                                  kDesiredAccess, nullptr));
      if (!desktop.IsValid()) {
        hr = HRESULT_FROM_WIN32(::GetLastError());
        LOGFN(ERROR) << "CreateDesktop hr=" << putHR(hr);
        return nullptr;
      }
    } else {
      LOGFN(ERROR) << "OpenDesktop hr=" << putHR(hr);
      return nullptr;
    }
  }

  PSECURITY_DESCRIPTOR sd;
  ACL* dacl;  // Not owned.
  DWORD err = ::GetSecurityInfo(desktop.Get(), SE_WINDOW_OBJECT,
                                DACL_SECURITY_INFORMATION, nullptr, nullptr,
                                &dacl, nullptr, &sd);
  if (err != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_NT(err);
    LOGFN(ERROR) << "GetSecurityInfo hr=" << putHR(hr);
    return nullptr;
  }

  // Add DACL entries.  This is the minimum set of access rights needed for
  // a simple MFC app to run.
  const DWORD kAccessMask =
      DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_HOOKCONTROL |
      DESKTOP_ENUMERATE | DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS |
      READ_CONTROL |
      // This permission is needed specifically by Chrome to run due to the
      // sandboxing it does with its processes.
      DESKTOP_SWITCHDESKTOP;
  ACL* new_dacl = nullptr;
  HRESULT hr = AddAllowedACE(dacl, 0, kAccessMask, sid, &new_dacl);
  ::LocalFree(sd);  // This "frees" dacl too.
  if (FAILED(hr)) {
    LOGFN(ERROR) << "AddAllowedACE 0 hr=" << putHR(hr);
    return nullptr;
  }

  err = ::SetSecurityInfo(desktop.Get(), SE_WINDOW_OBJECT,
                          DACL_SECURITY_INFORMATION, nullptr, nullptr, new_dacl,
                          nullptr);
  ::LocalFree(new_dacl);
  if (err != ERROR_SUCCESS) {
    hr = HRESULT_FROM_NT(err);
    LOGFN(ERROR) << "SetSecurityInfo hr=" << putHR(hr);
    return nullptr;
  }

  return desktop.Take();
}

// Sets up the minimum required privileges or ACLs needed for the logon SID
// to run the logon stub process in a restricted environment.
HRESULT SetupPermissionsForLogonSid(PSID sid) {
  HRESULT hr = AllowLogonSIDOnLocalBasedNamedObjects(sid);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "AllowLogonSIDOnLocalBasedNamedObjects hr=" << putHR(hr);
    return hr;
  }

  // Assume current window station is "WinSta0", which it should be for
  // winlogon.exe.
  if (kDesktopName[0] != 0) {
    // Add logon SID to the ACL of WinSta0.
    hr = AllowLogonSIDOnWinSta0(sid);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "AllowLogonSIDOnWinSta0 hr=" << putHR(hr);
      return hr;
    }

    ScopedDesktopHandle desktop;
    desktop.Set(
        GetAndAllowLogonSIDOnDesktop(kDesktopName, sid, DESKTOP_SWITCHDESKTOP));
    if (!desktop.IsValid()) {
      hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "GetAndAllowLogonSIDOnDesktop hr=" << putHR(hr);
      return hr;
    }
  }

  return S_OK;
}

}  // namespace

// static
OSProcessManager** OSProcessManager::GetInstanceStorage() {
  static OSProcessManager* instance = new OSProcessManager();
  return &instance;
}

// static
OSProcessManager* OSProcessManager::Get() {
  return *GetInstanceStorage();
}

// static
void OSProcessManager::SetInstanceForTesting(OSProcessManager* instance) {
  *GetInstanceStorage() = instance;
}

OSProcessManager::~OSProcessManager() {}

HRESULT OSProcessManager::GetTokenLogonSID(const base::win::ScopedHandle& token,
                                           PSID* sid) {
  return ::credential_provider::GetTokenLogonSID(token, sid);
}

HRESULT OSProcessManager::SetupPermissionsForLogonSid(PSID sid) {
  return ::credential_provider::SetupPermissionsForLogonSid(sid);
}

HRESULT OSProcessManager::CreateProcessWithToken(
    const base::win::ScopedHandle& logon_token,
    const base::CommandLine& command_line,
    _STARTUPINFOW* startupinfo,
    base::win::ScopedProcessInformation* procinfo) {
  // CreateProcessWithTokenW() expects the command line to be non-const, so make
  // a copy here.
  std::unique_ptr<wchar_t, void (*)(void*)>
      cmdline(wcsdup(command_line.GetCommandLineString().c_str()), std::free);
  PROCESS_INFORMATION temp_procinfo = {};
  if (!::CreateProcessWithTokenW(logon_token.Get(),
                                 LOGON_WITH_PROFILE,
                                 command_line.GetProgram().value().c_str(),
                                 cmdline.get(),
                                 CREATE_SUSPENDED,
                                 nullptr,  // environment
                                 nullptr,  // current directory
                                 startupinfo, &temp_procinfo)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    return hr;
  }
  procinfo->Set(temp_procinfo);
  return S_OK;
}

HRESULT OSProcessManager::CreateRunningProcess(
    const base::CommandLine& command_line,
    _STARTUPINFOW* startupinfo,
    base::win::ScopedProcessInformation* procinfo) {
  // command_line.GetCommandLineString() is not used here because it quotes the
  // command line to follow the command line rules of Microsoft C/C++ startup
  // code.  However this function is called to execute rundll32 which parses
  // command lines in a special way and fails when the first arg is double
  // quoted.  Therefore the command line is built manually here.
  std::wstring unquoted_cmdline =
      base::StrCat({L"\"", command_line.GetProgram().value(), L"\""});
  for (const auto& arg : command_line.GetArgs()) {
    unquoted_cmdline.append(FILE_PATH_LITERAL(" "));
    unquoted_cmdline.append(arg);
  }

  for (const auto& switch_value : command_line.GetSwitches()) {
    unquoted_cmdline.append(L" --");
    unquoted_cmdline.append(base::UTF8ToWide(switch_value.first));
    if (switch_value.second.empty())
      continue;
    unquoted_cmdline.append(L"=");
    unquoted_cmdline.append(switch_value.second);
  }

  base::LaunchOptions options;

  // If stdio handles are being passed to the process, make sure they are
  // included in the inherited list.  This assumes the handles are already
  // marked as inheritable.
  if ((startupinfo->dwFlags & STARTF_USESTDHANDLES) == STARTF_USESTDHANDLES) {
    options.stdin_handle = startupinfo->hStdInput;
    options.stdout_handle = startupinfo->hStdOutput;
    options.stderr_handle = startupinfo->hStdError;
    options.handles_to_inherit.push_back(startupinfo->hStdInput);
    options.handles_to_inherit.push_back(startupinfo->hStdOutput);
    options.handles_to_inherit.push_back(startupinfo->hStdError);
  }

  base::Process process(base::LaunchProcess(unquoted_cmdline, options));
  return process.IsValid() ? S_OK : E_FAIL;
}

}  // namespace credential_provider
