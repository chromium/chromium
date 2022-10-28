// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/updater/updater.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  // `argc` and `argv` are ignored by `base::CommandLine` for Windows. Instead,
  // the implementation parses `GetCommandLineW()` directly.
  return updater::UpdaterMain(0, nullptr);
}
