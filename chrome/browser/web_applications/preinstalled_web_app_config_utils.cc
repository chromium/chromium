// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_app_config_utils.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/common/chrome_paths_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {

namespace {

const base::FilePath* g_config_dir_for_testing = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The sub-directory of the extensions directory in which to scan for external
// web apps (as opposed to external extensions or external ARC apps).
const base::FilePath::CharType kWebAppsSubDirectory[] =
    FILE_PATH_LITERAL("web_apps");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
base::FilePath GetPreinstalledWebAppConfigDirFromDefaultPaths(
    Profile* profile) {
  if (g_config_dir_for_testing) {
    return *g_config_dir_for_testing;
  }

  base::FilePath web_apps_dir;
  if (chrome::GetPreinstalledWebAppConfigPath(&web_apps_dir))
    return web_apps_dir;
  return base::FilePath();
}

base::FilePath GetPreinstalledWebAppExtraConfigDirFromDefaultPaths(
    Profile* profile) {
  base::FilePath extra_web_apps_dir;
  if (chrome::GetPreinstalledWebAppExtraConfigPath(&extra_web_apps_dir))
    return extra_web_apps_dir;
  return base::FilePath();
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

const base::FilePath* GetPreinstalledWebAppConfigDirForTesting() {
  return g_config_dir_for_testing;
}

void SetPreinstalledWebAppConfigDirForTesting(
    const base::FilePath* config_dir) {
  g_config_dir_for_testing = config_dir;
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
base::FilePath GetPreinstalledWebAppConfigDirFromCommandLine(Profile* profile) {
  std::string command_line_directory =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPreinstalledWebAppsDir);
  if (!command_line_directory.empty())
    return base::FilePath::FromUTF8Unsafe(command_line_directory);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // As of mid 2018, only Chrome OS has default/external web apps, and
  // chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS is only defined for OS_LINUX,
  // which includes OS_CHROMEOS.

  // Exclude sign-in and lock screen profiles.
  if (!ash::ProfileHelper::IsUserProfile(profile)) {
    return {};
  }

  if (g_config_dir_for_testing) {
    return *g_config_dir_for_testing;
  }

  // For manual testing, you can change s/STANDALONE/USER/, as writing to
  // "$HOME/.config/chromium/test-user/.config/chromium/External
  // Extensions/web_apps" does not require root ACLs, unlike
  // "/usr/share/chromium/extensions/web_apps".
  base::FilePath dir;
  if (base::PathService::Get(chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS,
                             &dir)) {
    return dir.Append(kWebAppsSubDirectory);
  }

  LOG(ERROR) << "base::PathService::Get failed";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return {};
}

base::FilePath GetPreinstalledWebAppExtraConfigDirFromCommandLine(
    Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath config_dir =
      GetPreinstalledWebAppConfigDirFromCommandLine(profile);
  std::string extra_config_subdir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kExtraWebAppsDir);
  if (config_dir.empty() || extra_config_subdir.empty())
    return base::FilePath();
  return config_dir.AppendASCII(extra_config_subdir);
#else
  return base::FilePath();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

base::FilePath GetPreinstalledWebAppConfigDir(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return GetPreinstalledWebAppConfigDirFromDefaultPaths(profile);
#else
  return GetPreinstalledWebAppConfigDirFromCommandLine(profile);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

base::FilePath GetPreinstalledWebAppExtraConfigDir(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return GetPreinstalledWebAppExtraConfigDirFromDefaultPaths(profile);
#else
  return GetPreinstalledWebAppExtraConfigDirFromCommandLine(profile);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace web_app
