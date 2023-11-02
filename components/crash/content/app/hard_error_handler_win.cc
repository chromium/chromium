// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/hard_error_handler_win.h"

#include <DelayIMP.h>
#include <stddef.h>
#include <winternl.h>

#include "base/strings/string_util.h"
#include "components/crash/core/app/crash_reporter_client.h"

namespace breakpad {

using crash_reporter::GetCrashReporterClient;

namespace {
const DWORD kExceptionModuleNotFound = VcppException(ERROR_SEVERITY_ERROR,
                                                     ERROR_MOD_NOT_FOUND);
const DWORD kExceptionEntryPtNotFound = VcppException(ERROR_SEVERITY_ERROR,
                                                      ERROR_PROC_NOT_FOUND);
// This is defined in <ntstatus.h> but we can't include this file here.
const DWORD FACILITY_GRAPHICS_KERNEL = 0x1E;
const DWORD NT_STATUS_ENTRYPOINT_NOT_FOUND = 0xC0000139;
const DWORD NT_STATUS_DLL_NOT_FOUND = 0xC0000135;

// We assume that exception codes are NT_STATUS codes.
DWORD FacilityFromException(DWORD exception_code) {
  return (exception_code >> 16) & 0x0FFF;
}

// This is not a generic function. It only works with some |nt_status| values.
// Check the strings here http://msdn.microsoft.com/en-us/library/cc704588.aspx
// before attempting to use this function.
void RaiseHardErrorMsg(DWORD nt_status,
                       const std::string& p1,
                       const std::string& p2) {
  // If headless just exit silently.
  if (GetCrashReporterClient()->IsRunningUnattended())
    return;

  HMODULE ntdll = ::GetModuleHandleA("NTDLL.DLL");
  wchar_t* msg_template = nullptr;
  size_t count = ::FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS |
          FORMAT_MESSAGE_FROM_HMODULE,
      ntdll, nt_status, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<wchar_t*>(&msg_template), 0, nullptr);

  if (!count)
    return;
  count += p1.size() + p2.size() + 1;
  std::wstring message;
  ::wsprintf(base::WriteInto(&message, count), msg_template,
             p1.c_str(), p2.c_str());
  // The MB_SERVICE_NOTIFICATION causes this message to be displayed by
  // csrss. This means that we are not creating windows or pumping WM messages
  // in this process.
  ::MessageBox(nullptr, message.c_str(), L"chrome.exe",
               MB_OK | MB_SERVICE_NOTIFICATION);
  ::LocalFree(msg_template);
}

void ModuleNotFoundHardError(const EXCEPTION_RECORD* ex_record) {
  DelayLoadInfo* dli = reinterpret_cast<DelayLoadInfo*>(
      ex_record->ExceptionInformation[0]);
  if (!dli->szDll)
    return;
  RaiseHardErrorMsg(NT_STATUS_DLL_NOT_FOUND, dli->szDll, std::string());
}

void EntryPointNotFoundHardError(const EXCEPTION_RECORD* ex_record) {
  DelayLoadInfo* dli = reinterpret_cast<DelayLoadInfo*>(
      ex_record->ExceptionInformation[0]);
  if (!dli->dlp.fImportByName)
    return;
  if (!dli->dlp.szProcName)
    return;
  if (!dli->szDll)
    return;
  RaiseHardErrorMsg(NT_STATUS_ENTRYPOINT_NOT_FOUND,
      dli->dlp.szProcName, dli->szDll);
}

}  // namespace

bool HardErrorHandler(EXCEPTION_POINTERS* ex_info) {
  if (!ex_info)
    return false;
  if (!ex_info->ExceptionRecord)
    return false;

  DWORD exception = ex_info->ExceptionRecord->ExceptionCode;
  if (exception == kExceptionModuleNotFound) {
    ModuleNotFoundHardError(ex_info->ExceptionRecord);
    return true;
  }
  if (exception == kExceptionEntryPtNotFound) {
    EntryPointNotFoundHardError(ex_info->ExceptionRecord);
    return true;
  }
  if (FacilityFromException(exception) == FACILITY_GRAPHICS_KERNEL) {
    RaiseHardErrorMsg(exception, std::string(), std::string());
    return true;
  }
  return false;
}

}  // namespace breakpad
