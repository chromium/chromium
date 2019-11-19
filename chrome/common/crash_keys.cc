// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "content/public/common/content_switches.h"

#if defined(OS_CHROMEOS)
#include "chrome/common/chrome_switches.h"
#include "components/crash/content/app/crash_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_switches.h"
#endif

namespace crash_keys {

// Return true if we DON'T want to upload this flag to the crash server.
static bool IsBoringSwitch(const std::string& flag) {
  static const char* const kIgnoreSwitches[] = {
    switches::kEnableLogging,
    switches::kFlagSwitchesBegin,
    switches::kFlagSwitchesEnd,
    switches::kLoggingLevel,
    switches::kProcessType,
    switches::kV,
    switches::kVModule,
#if defined(OS_MACOSX)
    switches::kMetricsClientID,
#elif defined(OS_CHROMEOS)
    // --crash-loop-before is a "boring" switch because it is redundant;
    // crash_reporter separately informs the crash server if it is doing
    // crash-loop handling.
    crash_reporter::switches::kCrashLoopBefore,
    switches::kPpapiFlashArgs,
    switches::kPpapiFlashPath,
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

#if defined(OS_WIN)
  // Just about everything has this, don't bother.
  if (base::StartsWith(flag, "/prefetch:", base::CompareCase::SENSITIVE))
    return true;
#endif

  if (!base::StartsWith(flag, "--", base::CompareCase::SENSITIVE))
    return false;
  size_t end = flag.find("=");
  size_t len = (end == std::string::npos) ? flag.length() - 2 : end - 2;
  for (size_t i = 0; i < base::size(kIgnoreSwitches); ++i) {
    if (flag.compare(2, len, kIgnoreSwitches[i]) == 0)
      return true;
  }
  return false;
}

void SetCrashKeysFromCommandLine(const base::CommandLine& command_line) {
  return SetSwitchesFromCommandLine(command_line, &IsBoringSwitch);
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
  for (size_t i = 0; i < base::size(extension_ids); ++i) {
    if (it == extensions.end()) {
      extension_ids[i].Clear();
    } else {
      extension_ids[i].Set(*it);
      ++it;
    }
  }
}

}  // namespace crash_keys
