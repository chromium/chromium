// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/utils.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/security_interstitials/content/android/jni_headers/DateAndTimeSettingsHelper_jni.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/path_service.h"
#endif

namespace security_interstitials {

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void LaunchDateAndTimeSettings() {
// The code for each OS is completely separate, in order to avoid bugs like
// https://crbug.com/430877 .
#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DateAndTimeSettingsHelper_openDateAndTimeSettings(env);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  struct ClockCommand {
    const char* const pathname;
    const char* const argument;
  };
  static const ClockCommand kClockCommands[] = {
      // Unity
      {"/usr/bin/unity-control-center", "datetime"},
      // GNOME
      //
      // NOTE: On old Ubuntu, naming control panels doesn't work, so it
      // opens the overview. This will have to be good enough.
      {"/usr/bin/gnome-control-center", "datetime"},
      {"/usr/local/bin/gnome-control-center", "datetime"},
      {"/opt/bin/gnome-control-center", "datetime"},
      // KDE
      {"/usr/bin/kcmshell4", "clock"},
      {"/usr/local/bin/kcmshell4", "clock"},
      {"/opt/bin/kcmshell4", "clock"},
  };

  base::CommandLine command(base::FilePath(""));
  for (const ClockCommand& cmd : kClockCommands) {
    base::FilePath pathname(cmd.pathname);
    if (base::PathExists(pathname)) {
      command.SetProgram(pathname);
      command.AppendArg(cmd.argument);
      break;
    }
  }
  if (command.GetProgram().empty()) {
    // Alas, there is nothing we can do.
    return;
  }

  base::LaunchOptions options;
  options.wait = false;
  options.allow_new_privs = true;
  base::LaunchProcess(command, options);

#elif BUILDFLAG(IS_MAC)
  base::mac::OpenSystemSettingsPane(base::mac::SystemSettingsPane::kDateTime);
#elif BUILDFLAG(IS_WIN)
  base::FilePath path;
  base::PathService::Get(base::DIR_SYSTEM, &path);
  static const wchar_t kControlPanelExe[] = L"control.exe";
  path = path.Append(std::wstring(kControlPanelExe));
  base::CommandLine command(path);
  command.AppendArg(std::string("/name"));
  command.AppendArg(std::string("Microsoft.DateAndTime"));

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
