// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/utils.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/notimplemented.h"
#include "base/process/launch.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/security_interstitials/content/android/jni_headers/DateAndTimeSettingsHelper_jni.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/path_service.h"
#endif

namespace security_interstitials {

#if BUILDFLAG(IS_LINUX)
namespace {

using Command = base::span<const char* const>;

constexpr const char* kUnityClockCommand[] = {"unity-control-center",
                                              "datetime"};
// NOTE: On old Ubuntu, naming control panels doesn't work, so it opens the
// overview. This will have to be good enough.
constexpr const char* kGnomeClockCommand[] = {"gnome-control-center",
                                              "datetime"};
constexpr const char* kKde4ClockCommand[] = {"kcmshell4", "clock"};
constexpr const char* kKde5ClockCommand[] = {"kcmshell5", "clock"};
constexpr const char* kKde6ClockCommand[] = {"kcmshell6", "kcm_clock"};

}  // namespace

std::vector<std::string> GetDateAndTimeSettingsCommand(base::Environment* env) {
  std::vector<Command> candidates;
  switch (base::nix::GetDesktopEnvironment(env)) {
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      candidates.push_back(kKde4ClockCommand);
      break;
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
      candidates.push_back(kKde5ClockCommand);
      break;
    case base::nix::DESKTOP_ENVIRONMENT_KDE6:
      candidates.push_back(kKde6ClockCommand);
      break;
    default:
      break;
  }
  // Fall back to any known tool, in case the desktop environment is unknown
  // or its preferred tool is not installed.
  candidates.insert(candidates.end(),
                    {kUnityClockCommand, kGnomeClockCommand, kKde6ClockCommand,
                     kKde5ClockCommand, kKde4ClockCommand});

  for (Command command : candidates) {
    if (base::ExecutableExistsInPath(env, command[0])) {
      return std::vector<std::string>(command.begin(), command.end());
    }
  }
  return {};
}
#endif  // BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_CHROMEOS)
void LaunchDateAndTimeSettings() {
// The code for each OS is completely separate, in order to avoid bugs like
// https://crbug.com/430877 .
#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DateAndTimeSettingsHelper_openDateAndTimeSettings(env);
#elif BUILDFLAG(IS_LINUX)
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::vector<std::string> argv = GetDateAndTimeSettingsCommand(env.get());
  if (argv.empty()) {
    // Alas, there is nothing that can be done.
    return;
  }

  base::LaunchOptions options;
  options.wait = false;
  options.allow_new_privs = true;
  base::LaunchProcess(argv, options);

#elif BUILDFLAG(IS_MAC)
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kGeneral_DateTime);
#elif BUILDFLAG(IS_WIN)
  base::FilePath path;
  base::PathService::Get(base::DIR_SYSTEM, &path);
  static const wchar_t kControlPanelExe[] = L"control.exe";
  path = path.Append(kControlPanelExe);
  base::CommandLine command(path);
  command.AppendArg("/name");
  command.AppendArg("Microsoft.DateAndTime");

  base::LaunchOptions options;
  options.wait = false;
  base::LaunchProcess(command, options);

#elif BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
  // TODO(crbug.com/40191566): Send to the platform settings.
  // The iOS Blink port also need to send the platform settings.
  NOTIMPLEMENTED_LOG_ONCE();
#else
#error Unsupported target architecture.
#endif
  // Don't add code here! (See the comment at the beginning of the function.)
}
#endif

}  // namespace security_interstitials

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(DateAndTimeSettingsHelper)
#endif
