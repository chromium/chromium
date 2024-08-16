// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/common/crash_keys.h"

#include <deque>
#include <string_view>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
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

constexpr std::string_view kStringAnnotationsSwitch = "string-annotations";

// A convenient wrapper around a crash key and its name.
//
// The CrashKey contract requires that CrashKeyStrings are never
// moved, copied, or deleted (see
// third_party/crashpad/crashpad/client/annotation.h); since this class holds
// a CrashKeyString, it likewise cannot be moved, copied, or deleted.
class CrashKeyWithName {
 public:
  explicit CrashKeyWithName(std::string name)
      : name_(std::move(name)), crash_key_(name_.c_str()) {}
  CrashKeyWithName(const CrashKeyWithName&) = delete;
  CrashKeyWithName& operator=(const CrashKeyWithName&) = delete;
  CrashKeyWithName(CrashKeyWithName&&) = delete;
  CrashKeyWithName& operator=(CrashKeyWithName&&) = delete;
  ~CrashKeyWithName() = delete;

  std::string_view Name() const { return name_; }
  std::string_view Value() const { return crash_key_.value(); }
  void Clear() { crash_key_.Clear(); }
  void Set(std::string_view value) { crash_key_.Set(value); }

 private:
  std::string name_;
  crash_reporter::CrashKeyString<64> crash_key_;
};

void SplitAndPopulateCrashKeys(std::deque<CrashKeyWithName>& crash_keys,
                               std::string_view comma_separated_feature_list,
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
  static const std::string_view kIgnoreSwitches[] = {
    kStringAnnotationsSwitch,
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
    "enterprise-enable-forced-re-enrollment-on-flex",
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

std::deque<CrashKeyWithName>& GetCommandLineStringAnnotations() {
  static base::NoDestructor<std::deque<CrashKeyWithName>>
      command_line_string_annotations;
  return *command_line_string_annotations;
}

void SetStringAnnotations(const base::CommandLine& command_line) {
  // This is only meant to be used to pass annotations from the browser to
  // children and not to be used on the browser command line.
  if (!command_line.HasSwitch(switches::kProcessType)) {
    return;
  }
  base::StringPairs annotations;
  if (!base::SplitStringIntoKeyValuePairs(
          command_line.GetSwitchValueASCII(kStringAnnotationsSwitch), '=', ',',
          &annotations)) {
    return;
  }
  for (const auto& [key, value] : annotations) {
    GetCommandLineStringAnnotations().emplace_back(key).Set(value);
  }
}

}  // namespace

void AllocateCrashKeyInBrowserAndChildren(std::string_view key,
                                          std::string_view value) {
  GetCommandLineStringAnnotations().emplace_back(std::string(key)).Set(value);
}

void AppendStringAnnotationsCommandLineSwitch(base::CommandLine* command_line) {
  std::string string_annotations;
  for (const auto& crash_key : GetCommandLineStringAnnotations()) {
    if (!string_annotations.empty()) {
      string_annotations.push_back(',');
    }
    string_annotations = base::StrCat(
        {string_annotations, crash_key.Name(), "=", crash_key.Value()});
  }
  command_line->AppendSwitchASCII(kStringAnnotationsSwitch, string_annotations);
}

void SetCrashKeysFromCommandLine(const base::CommandLine& command_line) {
  SetStringAnnotations(command_line);
  HandleEnableDisableFeatures(command_line);
  SetSwitchesFromCommandLine(command_line, &IsBoringSwitch);
}

}  // namespace crash_keys
