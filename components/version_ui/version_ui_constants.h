// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VERSION_UI_VERSION_UI_CONSTANTS_H_
#define COMPONENTS_VERSION_UI_VERSION_UI_CONSTANTS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace version_ui {

// Message handlers.
// Must match the constants used in the resource files.
extern const char kRequestVersionInfo[];
extern const char kRequestVariationInfo[];
extern const char kRequestPathInfo[];

extern const char kKeyVariationsList[];
extern const char kKeyVariationsCmd[];
extern const char kKeyExecPath[];
extern const char kKeyProfilePath[];

// Strings.
// Must match the constants used in the resource files.
extern const char kApplicationLabel[];
#if BUILDFLAG(IS_CHROMEOS)
extern const char kARC[];
#endif
extern const char kCL[];
extern const char kCommandLine[];
extern const char kCommandLineName[];
extern const char kCompany[];
#if BUILDFLAG(IS_WIN)
extern const char kUpdateCohortName[];
#endif
extern const char kCopyright[];
#if BUILDFLAG(IS_CHROMEOS)
extern const char kCustomizationId[];
#endif
#if !BUILDFLAG(IS_IOS)
extern const char kExecutablePath[];
extern const char kExecutablePathName[];
#endif
#if BUILDFLAG(IS_CHROMEOS)
extern const char kFirmwareVersion[];
#endif
#if !BUILDFLAG(IS_IOS)
extern const char kJSEngine[];
extern const char kJSVersion[];
#endif
extern const char kLogoAltText[];
extern const char kOfficial[];
#if !BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kOSName[];
extern const char kOSType[];
#endif
#if BUILDFLAG(IS_ANDROID)
extern const char kOSVersion[];
extern const char kVersionCode[];
extern const char kTargetSdkVersionName[];
extern const char kTargetSdkVersion[];
extern const char kTargetsUName[];
extern const char kTargetsU[];
extern const char kGmsName[];
extern const char kGmsVersion[];
#endif
#if BUILDFLAG(IS_CHROMEOS)
extern const char kPlatform[];
#endif
#if !BUILDFLAG(IS_IOS)
extern const char kProfilePath[];
extern const char kProfilePathName[];
#endif
#if BUILDFLAG(IS_CHROMEOS)
extern const char kOsVersionHeaderText1[];
extern const char kOsVersionHeaderText2[];
extern const char kOsVersionHeaderLink[];
#endif
extern const char kCopyLabel[];
extern const char kCopyNotice[];
extern const char kRevision[];
extern const char kSanitizer[];
extern const char kTitle[];
extern const char kUserAgent[];
extern const char kUserAgentName[];
extern const char kVariationsCmdName[];
extern const char kVariationsName[];
extern const char kVariationsSeed[];
extern const char kVariationsSeedName[];
extern const char kVersion[];
extern const char kVersionModifier[];
extern const char kVersionProcessorVariation[];

}  // namespace version_ui

#endif  // COMPONENTS_VERSION_UI_VERSION_UI_CONSTANTS_H_
