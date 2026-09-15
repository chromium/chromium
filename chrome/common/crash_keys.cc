// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#include <array>
#include <deque>
#include <string_view>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/access_token.h"
#include "chrome/installer/util/isolation_support.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "components/crash/core/app/crash_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_switches.h"
#endif

namespace crash_keys {
namespace {

constexpr std::string_view kStringAnnotationsSwitch = "string-annotations";

// Return true if we DON'T want to upload this flag to the crash server.
bool IsBoringSwitch(const std::string& flag) {
  if (crash_keys::IsDefaultBoringSwitch(flag)) {
    return true;
  }

  static const auto kIgnoreSwitches = std::to_array<std::string_view>({
      kStringAnnotationsSwitch,
      switches::kEnableLogging,
      switches::kLoggingLevel,
      switches::kProcessType,
      switches::kV,
      switches::kVModule,
      // This is a serialized buffer which won't fit in the default 64 bytes
      // anyways. Should be switches::kGpuPreferences but we run into linking
      // errors on Windows if we try to use that directly.
      "gpu-preferences",
#if BUILDFLAG(IS_MAC)
      switches::kMetricsClientID,
#elif BUILDFLAG(IS_CHROMEOS)
      // --crash-loop-before is a "boring" switch because it is redundant;
      // crash_reporter separately informs the crash server if it is doing
      // crash-loop handling.
      crash_reporter::switches::kCrashLoopBefore,
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
  });

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
  if (string_annotations.empty()) {
    return;
  }
  command_line->AppendSwitchASCII(kStringAnnotationsSwitch, string_annotations);
}

void SetCrashKeysFromCommandLine(const base::CommandLine& command_line) {
  SetStringAnnotations(command_line);
  SetFeaturesFromCommandLine(command_line);
  SetSwitchesFromCommandLine(command_line, &IsBoringSwitch);

#if BUILDFLAG(IS_WIN)
  auto process_token = base::win::AccessToken::FromCurrentProcess();
  if (process_token && process_token->GetSecurityAttribute(
                           installer::GetIsolationAttributeName())) {
    static crash_reporter::CrashKeyString<32> is_isolated("is-isolated");
    is_isolated.Set("yes");
  }
#endif
}

}  // namespace crash_keys
