// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WIN_CORE_DLLS_H_
#define CHROME_COMMON_WIN_CORE_DLLS_H_

#include <array>

#include "base/strings/cstring_view.h"

// DLLs delay-loaded by chrome.dll whose imports may all be resolved at startup
// in the browser process. A function that one of these DLLs doesn't export on
// the earliest supported Windows version must be resolved with GetProcAddress()
// instead of being imported (enforced by delayloads_unittests).
//
// Each name must match the casing in chrome.dll's delay-import directory:
// `base::win::LoadAllImportsForDll()` looks DLLs up case-sensitively, so a
// mismatch would silently skip resolving that DLL at startup.
inline constexpr auto kCoreDlls = std::to_array<base::cstring_view>({
    "USER32.dll",
    "dwmapi.dll",
    "UxTheme.dll",
    "GDI32.dll",
    "IMM32.dll",
});

#endif  // CHROME_COMMON_WIN_CORE_DLLS_H_
