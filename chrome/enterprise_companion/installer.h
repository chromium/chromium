// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_INSTALLER_H_
#define CHROME_ENTERPRISE_COMPANION_INSTALLER_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#endif  // BUILDFLAG(IS_WIN)

namespace enterprise_companion {

#if BUILDFLAG(IS_WIN)
// The registry key in which the companion app's updater registration is stored.
inline constexpr wchar_t kAppRegKey[] =
    L"Software\\" COMPANY_SHORTNAME_STRING
    "\\Update\\Clients\\" ENTERPRISE_COMPANION_APPID;
// The registry value under `kAppRegKey` which stores the application's version.
inline constexpr wchar_t kRegValuePV[] = L"pv";
// The registry value under `kAppRegKey` which stores the application's name.
inline constexpr wchar_t kRegValueName[] = L"name";

#if ENTERPRISE_COMPANION_USE_ICU_DATA_FILE
inline constexpr wchar_t kIcuDataFileName[] = L"icudtl.dat";
#endif  // ENTERPRISE_COMPANION_USE_ICU_DATA_FILE

#endif  // BUILDFLAG(IS_WIN)

// Install the Chrome Enterprise Companion App.
bool Install();

// Remove all traces of the app from the system. On Windows this includes
// removing the installation for the alternate architecture, if present.
bool Uninstall();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_INSTALLER_H_
