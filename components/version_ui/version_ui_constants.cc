// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/version_ui/version_ui_constants.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace version_ui {

// Message handlers.
const char kRequestVersionInfo[] = "requestVersionInfo";
const char kRequestVariationInfo[] = "requestVariationInfo";
const char kRequestPathInfo[] = "requestPathInfo";

// Named keys used in message handler responses.
const char kKeyVariationsList[] = "variationsList";
const char kKeyVariationsCmd[] = "variationsCmd";
const char kKeyExecPath[] = "execPath";
const char kKeyProfilePath[] = "profilePath";

// Strings.
const char kApplicationLabel[] = "application_label";
#if BUILDFLAG(IS_CHROMEOS)
const char kARC[] = "arc_label";
#endif
const char kCL[] = "cl";
const char kCommandLine[] = "command_line";
const char kCommandLineName[] = "command_line_name";
const char kCompany[] = "company";
#if BUILDFLAG(IS_WIN)
const char kUpdateCohortName[] = "update_cohort_name";
#endif
const char kCopyright[] = "copyright";
#if BUILDFLAG(IS_CHROMEOS)
const char kCustomizationId[] = "customization_id";
#endif
#if !BUILDFLAG(IS_IOS)
const char kExecutablePath[] = "executable_path";
const char kExecutablePathName[] = "executable_path_name";
#endif
#if BUILDFLAG(IS_CHROMEOS)
const char kFirmwareVersion[] = "firmware_version";
#endif
#if !BUILDFLAG(IS_IOS)
const char kJSEngine[] = "js_engine";
const char kJSVersion[] = "js_version";
#endif
const char kLogoAltText[] = "logo_alt_text";
const char kOfficial[] = "official";
#if !BUILDFLAG(IS_CHROMEOS_ASH)
const char kOSName[] = "os_name";
const char kOSType[] = "os_type";
#endif
#if BUILDFLAG(IS_ANDROID)
const char kOSVersion[] = "os_version";
const char kVersionCode[] = "version_code";
const char kTargetSdkVersionName[] = "target_sdk_version_name";
const char kTargetSdkVersion[] = "target_sdk_version";
const char kTargetsUName[] = "targets_u_name";
const char kTargetsU[] = "targets_u";
const char kGmsName[] = "gms_name";
const char kGmsVersion[] = "gms_version";
#endif
#if BUILDFLAG(IS_CHROMEOS)
const char kPlatform[] = "platform";
#endif
#if !BUILDFLAG(IS_IOS)
const char kProfilePath[] = "profile_path";
const char kProfilePathName[] = "profile_path_name";
#endif
#if BUILDFLAG(IS_CHROMEOS)
const char kOsVersionHeaderText1[] = "os-version-text1";
const char kOsVersionHeaderText2[] = "os-version-text2";
const char kOsVersionHeaderLink[] = "os-version-link";
#endif
const char kCopyLabel[] = "copy_label";
const char kCopyNotice[] = "copy_notice";
const char kRevision[] = "revision";
const char kSanitizer[] = "sanitizer";
const char kTitle[] = "title";
const char kUserAgent[] = "useragent";
const char kUserAgentName[] = "user_agent_name";
const char kVariationsCmdName[] = "variations_cmd_name";
const char kVariationsName[] = "variations_name";
const char kVariationsSeed[] = "variations_seed";
const char kVariationsSeedName[] = "variations_seed_name";
const char kVersion[] = "version";
const char kVersionModifier[] = "version_modifier";
const char kVersionProcessorVariation[] = "version_processor_variation";

}  // namespace version_ui
