// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include <deque>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/crash/core/app/crash_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_switches.h"
#endif

namespace crash_keys {
namespace {

// A convenient wrapper around a crash key and its name.
class CrashKeyWithName {
 public:
  explicit CrashKeyWithName(std::string name)
      : name_(std::move(name)), crash_key_(name_.c_str()) {}

  void Clear() { crash_key_.Clear(); }
  void Set(base::StringPiece value) { crash_key_.Set(value); }

 private:
  std::string name_;
  crash_reporter::CrashKeyString<64> crash_key_;
};

void SplitAndPopulateCrashKeys(std::deque<CrashKeyWithName>& crash_keys,
                               base::StringPiece comma_separated_feature_list,
                               std::string crash_key_name_prefix) {
  // Crash keys are indestructable so we can not simply empty the deque.
  // Instead we must keep the previous crash keys alive and clear their values.
  for (CrashKeyWithName& crash_key : crash_keys)
    crash_key.Clear();

  auto features =
      base::SplitString(comma_separated_feature_list, ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (size_t i = 0; i < features.size(); i++) {
    if (crash_keys.size() <= i) {
      crash_keys.emplace_back(base::StringPrintf(
          "%s-%" PRIuS, crash_key_name_prefix.c_str(), i + 1));
    }

    CrashKeyWithName& crash_key = crash_keys[i];
    crash_key.Set(features[i]);
  }
}

// --enable-features and --disable-features often contain a long list not
// fitting into 64 bytes, hiding important information when analysing crashes.
// Therefore they are separated out in a list of CrashKeys, one for each enabled
// or disabled feature.
// They are also excluded from the default "switches".
void HandleEnableDisableFeatures(const base::CommandLine& command_line) {
  static base::NoDestructor<std::deque<CrashKeyWithName>>
      enabled_features_crash_keys;
  static base::NoDestructor<std::deque<CrashKeyWithName>>
      disabled_features_crash_keys;

  SplitAndPopulateCrashKeys(
      *enabled_features_crash_keys,
      command_line.GetSwitchValueASCII(switches::kEnableFeatures),
      "commandline-enabled-feature");

  SplitAndPopulateCrashKeys(
      *disabled_features_crash_keys,
      command_line.GetSwitchValueASCII(switches::kDisableFeatures),
      "commandline-disabled-feature");
}

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
    switches::kEnableFeatures,
    switches::kDisableFeatures,
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
  HandleEnableDisableFeatures(command_line);
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
