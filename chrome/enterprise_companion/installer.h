// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_INSTALLER_H_
#define CHROME_ENTERPRISE_COMPANION_INSTALLER_H_

#include "build/build_config.h"

namespace enterprise_companion {

#if BUILDFLAG(IS_WIN)
// The registry key in which the companion app's updater registration is stored.
extern const wchar_t kAppRegKey[];
// The registry value under `kAppRegKey` which stores the application's version.
extern const wchar_t kRegValuePV[];
// The registry value under `kAppRegKey` which stores the application's name.
extern const wchar_t kRegValueName[];
#endif

// Install the Chrome Enterprise Companion App.
bool Install();

// Remove all traces of the app from the system. On Windows this includes
// removing the installation for the alternate architecture, if present.
bool Uninstall();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_INSTALLER_H_
