// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/common/chrome_switches.h"
#include "components/crash/core/app/crash_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_switches.h"
#endif

namespace crash_keys {
namespace {

#if BUILDFLAG(IS_CHROMEOS)
// ChromeOS uses --enable-features and --disable-features more heavily than
// most platforms, and the results don't fit into the default 64 bytes. So they
// are listed in special, larger CrashKeys and excluded from the default
// "switches".
void HandleEnableDisableFeatures(const base::CommandLine& command_line) {
  static crash_reporter::CrashKeyString<150> enable_features_key(
      "commandline-enabled-features");
  enable_features_key.Set(
      command_line.GetSwitchValueASCII(switches::kEnableFeatures));

  static crash_reporter::CrashKeyString<150> disable_features_key(
      "commandline-disabled-features");
  disable_features_key.Set(
      command_line.GetSwitchValueASCII(switches::kDisableFeatures));
}
#endif

// Return true if we DON'T want to upload this flag to the crash server.
bool IsBoringSwitch(const std::string& flag) {
  static const char* const kIgnoreSwitches[] = {
    switches::kEnableLogging,
    switches::kFlagSwitchesBegin,
    switches::kFlagSwitchesEnd,
    switches::kLoggingLevel,
    switches::kProcessType,
    switches::kV,
    switches::kVModule,
    // This is a serialized buffer which won't fit in the default 64 bytes
    // anyways. Should be switches::kGpuPreferences but we run into linking
    // errors on Windows if we try to use that directly.
    "gpu-preferences",
#if BUILDFLAG(IS_CHROMEOS)
    switches::kEnableFeatures,
    switches::kDisableFeatures,
#endif
#if BUILDFLAG(IS_MAC)
    switches::kMetricsClientID,
#elif BUILDFLAG(IS_CHROMEOS_ASH)
    // --crash-loop-before is a "boring" switch because it is redundant;
    // crash_reporter separately informs the crash server if it is doing
    // crash-loop handling.
    crash_reporter::switches::kCrashLoopBefore,
    switches::kRegisterPepperPlugins,
    switches::kUseGL,
    switches::kUserDataDir,
    // Cros/CC flags are specified as raw strings to avoid dependency.
    "child-wallpaper-large",
    "child-wallpaper-small",
    "default-wallpaper-large",
    "default-wallpaper-small",
    "guest-wallpaper-large",
    "guest-wallpaper-small",
    "enterprise-enable-forced-re-enrollment",
    "enterprise-enrollment-initial-modulus",
    "enterprise-enrollment-modulus-limit",
    "login-profile",
    "login-user",
    "use-cras",
#endif
  };

#if BUILDFLAG(IS_WIN)
  // Just about everything has this, don't bother.
  if (base::StartsWith(flag, "/prefetch:", base::CompareCase::SENSITIVE))
    return true;
#endif

  if (!base::StartsWith(flag, "--", base::CompareCase::SENSITIVE))
    return false;
  size_t end = flag.find("=");
  size_t len = (end == std::string::npos) ? flag.length() - 2 : end - 2;
  for (size_t i = 0; i < std::size(kIgnoreSwitches); ++i) {
    if (flag.compare(2, len, kIgnoreSwitches[i]) == 0)
      return true;
  }
  return false;
}

}  // namespace

void SetCrashKeysFromCommandLine(const base::CommandLine& command_line) {
#if BUILDFLAG(IS_CHROMEOS)
  HandleEnableDisableFeatures(command_line);
#endif
  SetSwitchesFromCommandLine(command_line, &IsBoringSwitch);
}

void SetActiveExtensions(const std::set<std::string>& extensions) {
  static crash_reporter::CrashKeyString<4> num_extensions("num-extensions");
  num_extensions.Set(base::NumberToString(extensions.size()));

  using ExtensionIDKey = crash_reporter::CrashKeyString<64>;
  static ExtensionIDKey extension_ids[] = {
      {"extension-1", ExtensionIDKey::Tag::kArray},
      {"extension-2", ExtensionIDKey::Tag::kArray},
      {"extension-3", ExtensionIDKey::Tag::kArray},
      {"extension-4", ExtensionIDKey::Tag::kArray},
      {"extension-5", ExtensionIDKey::Tag::kArray},
      {"extension-6", ExtensionIDKey::Tag::kArray},
      {"extension-7", ExtensionIDKey::Tag::kArray},
      {"extension-8", ExtensionIDKey::Tag::kArray},
      {"extension-9", ExtensionIDKey::Tag::kArray},
      {"extension-10", ExtensionIDKey::Tag::kArray},
  };

  auto it = extensions.begin();
  for (size_t i = 0; i < std::size(extension_ids); ++i) {
    if (it == extensions.end()) {
      extension_ids[i].Clear();
    } else {
      extension_ids[i].Set(*it);
      ++it;
    }
  }
}

}  // namespace crash_keys
