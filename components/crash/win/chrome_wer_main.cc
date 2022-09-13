// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Windows.h>

// werapi.h must be after Windows.h
#include <werapi.h>

#include "third_party/crashpad/crashpad/handler/win/wer/crashpad_wer.h"

extern "C" {
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  return true;
}

HRESULT OutOfProcessExceptionEventCallback(
    PVOID pContext,
    const PWER_RUNTIME_EXCEPTION_INFORMATION pExceptionInformation,
    BOOL* pbOwnershipClaimed,
    PWSTR pwszEventName,
    PDWORD pchSize,
    PDWORD pdwSignatureCount) {
  // Exceptions that are not collected by crashpad's in-process handlers.
  DWORD wanted_exceptions[1] = {
      0xC0000409,  // STATUS_STACK_BUFFER_OVERRUN
  };
  bool result = crashpad::wer::ExceptionEvent(
      wanted_exceptions, sizeof(wanted_exceptions) / sizeof(DWORD), pContext,
      pExceptionInformation);

  if (result) {
    *pbOwnershipClaimed = TRUE;
    // Technically we failed as we terminated the process.
    return E_FAIL;
  }
  // Could not dump for whatever reason, so let other helpers/wer have a chance.
  *pbOwnershipClaimed = FALSE;
  return S_OK;
}

HRESULT OutOfProcessExceptionEventSignatureCallback(
    PVOID pContext,
    const PWER_RUNTIME_EXCEPTION_INFORMATION pExceptionInformation,
    DWORD dwIndex,
    PWSTR pwszName,
    PDWORD pchName,
    PWSTR pwszValue,
    PDWORD pchValue) {
  // This function should never be called.
  return E_FAIL;
}

HRESULT OutOfProcessExceptionEventDebuggerLaunchCallback(
    PVOID pContext,
    const PWER_RUNTIME_EXCEPTION_INFORMATION pExceptionInformation,
    PBOOL pbIsCustomDebugger,
    PWSTR pwszDebuggerLaunch,
    PDWORD pchDebuggerLaunch,
    PBOOL pbIsDebuggerAutolaunch) {
  // This function should never be called.
  return E_FAIL;
}
}  // extern "C"
