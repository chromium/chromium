// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome installer.

#ifndef CHROME_INSTALLER_SETUP_SETUP_CONSTANTS_H_
#define CHROME_INSTALLER_SETUP_SETUP_CONSTANTS_H_

#include <string_view>

#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace installer {

extern const wchar_t kChromeArchive[];
extern const wchar_t kChromeCompressedArchive[];
extern const char kVisualElements[];
extern const wchar_t kVisualElementsManifest[];

extern const wchar_t kInstallSourceDir[];
extern const wchar_t kInstallSourceChromeDir[];

extern const wchar_t kMediaPlayerRegPath[];

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const wchar_t kOsUpdateHandlerExe[];
// Switch that forces the installer to skip OS upgrade checks.
extern const char kPEHForceInstall[];
#endif

inline constexpr std::wstring_view kElevatedTracingServiceExe =
    L"elevated_tracing_service.exe";

namespace switches {

extern const char kCleanupForDowngradeOperation[];
extern const char kCleanupForDowngradeVersion[];

extern const char kConfigureBrowserInDirectory[];

inline constexpr std::string_view kDeveloper = "developer";

extern const char kSetDisplayVersionProduct[];
extern const char kSetDisplayVersionValue[];
extern const char kStartupEventHandle[];

}  // namespace switches

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_SETUP_CONSTANTS_H_
