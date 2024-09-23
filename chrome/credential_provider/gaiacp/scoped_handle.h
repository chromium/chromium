// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_SCOPED_HANDLE_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_SCOPED_HANDLE_H_

#include <windows.h>

#include <winhttp.h>

#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"

namespace credential_provider {

// Window station scoped handle.

class WindowStationTraits {
 public:
  using Handle = HWINSTA;

  WindowStationTraits() = delete;
  WindowStationTraits(const WindowStationTraits&) = delete;
  WindowStationTraits& operator=(const WindowStationTraits&) = delete;

  static bool CloseHandle(HWINSTA handle) {
    return ::CloseWindowStation(handle) != FALSE;
  }

  static bool IsHandleValid(HWINSTA handle) {
    return handle != nullptr;
  }

  static HWINSTA NullHandle() {
    return nullptr;
  }
};

typedef base::win::GenericScopedHandle<WindowStationTraits,
                                       base::win::DummyVerifierTraits>
    ScopedWindowStationHandle;

// Desktop scoped handle.

class DesktopTraits {
 public:
  using Handle = HDESK;

  DesktopTraits() = delete;
  DesktopTraits(const DesktopTraits&) = delete;
  DesktopTraits& operator=(const DesktopTraits&) = delete;

  static bool CloseHandle(HDESK handle) {
    return ::CloseDesktop(handle) != FALSE;
  }

  static bool IsHandleValid(HDESK handle) {
    return handle != nullptr;
  }

  static HDESK NullHandle() {
    return nullptr;
  }
};

typedef base::win::GenericScopedHandle<DesktopTraits,
                                       base::win::DummyVerifierTraits>
    ScopedDesktopHandle;

// WinHttp scoped handle.

class WinHttpTraits {
 public:
  using Handle = HINTERNET;

  WinHttpTraits() = delete;
  WinHttpTraits(const WinHttpTraits&) = delete;
  WinHttpTraits& operator=(const WinHttpTraits&) = delete;

  static bool CloseHandle(HINTERNET handle) {
    return ::WinHttpCloseHandle(handle) != FALSE;
  }

  static bool IsHandleValid(HINTERNET handle) {
    return handle != nullptr;
  }

  static HINTERNET NullHandle() {
    return nullptr;
  }
};

typedef base::win::GenericScopedHandle<WinHttpTraits,
                                       base::win::DummyVerifierTraits>
    ScopedWinHttpHandle;

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_SCOPED_HANDLE_H_
