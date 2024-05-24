// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_POSIX)
int main(int argc, const char* argv[]) {
  return enterprise_companion::EnterpriseCompanionMain(argc, argv);
}
#elif BUILDFLAG(IS_WIN)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  // `argc` and `argv` are ignored by `base::CommandLine` for Windows. Instead,
  // the implementation parses `GetCommandLineW()` directly.
  return enterprise_companion::EnterpriseCompanionMain(0, nullptr);
}
#endif
