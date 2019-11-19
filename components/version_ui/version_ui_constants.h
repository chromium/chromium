// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VERSION_UI_VERSION_UI_CONSTANTS_H_
#define COMPONENTS_VERSION_UI_VERSION_UI_CONSTANTS_H_

#include "build/build_config.h"

namespace version_ui {

// Resource paths.
// Must match the resource file names.
extern const char kAboutVersionCSS[];
extern const char kVersionJS[];

// Message handlers.
// Must match the constants used in the resource files.
extern const char kRequestVersionInfo[];
extern const char kRequestVariationInfo[];
extern const char kRequestPluginInfo[];
extern const char kRequestPathInfo[];

extern const char kKeyVariationsList[];
extern const char kKeyVariationsCmd[];
extern const char kKeyExecPath[];
extern const char kKeyProfilePath[];

// Strings.
// Must match the constants used in the resource files.
extern const char kApplicationLabel[];
#if defined(OS_CHROMEOS)
extern const char kARC[];
#endif
extern const char kCL[];
extern const char kCommandLine[];
extern const char kCommandLineName[];
extern const char kCompany[];
#if defined(OS_IOS)
extern const char kCompiler[];
#endif
#if defined(OS_WIN)
extern const char kUpdateCohortName[];
#endif
extern const char kCopyright[];
#if defined(OS_CHROMEOS)
extern const char kCustomizationId[];
#endif
#if !defined(OS_IOS)
extern const char kExecutablePath[];
extern const char kExecutablePathName[];
#endif
#if defined(OS_CHROMEOS)
extern const char kFirmwareVersion[];
#endif
#if !defined(OS_ANDROID) && !defined(OS_IOS)
extern const char kFlashPlugin[];
extern const char kFlashVersion[];
#endif
#if !defined(OS_IOS)
extern const char kJSEngine[];
extern const char kJSVersion[];
#endif
extern const char kOfficial[];
#if !defined(OS_CHROMEOS)
extern const char kOSName[];
extern const char kOSType[];
#endif
#if defined(OS_ANDROID)
extern const char kOSVersion[];
extern const char kGmsName[];
extern const char kGmsVersion[];
#endif
#if defined(OS_CHROMEOS)
extern const char kPlatform[];
#endif
#if !defined(OS_IOS)
extern const char kProfilePath[];
extern const char kProfilePathName[];
#endif
extern const char kRevision[];
extern const char kSanitizer[];
extern const char kTitle[];
extern const char kUserAgent[];
extern const char kUserAgentName[];
extern const char kVariationsCmdName[];
extern const char kVariationsName[];
extern const char kVersion[];
extern const char kVersionBitSize[];
extern const char kVersionModifier[];

}  // namespace version_ui

#endif  // COMPONENTS_VERSION_UI_VERSION_UI_CONSTANTS_H_
