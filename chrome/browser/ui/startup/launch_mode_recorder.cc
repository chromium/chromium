// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/launch_mode_recorder.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/mac/dock.h"
#include "chrome/browser/mac/install_from_dmg.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/win/startup_information.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/taskbar_util.h"
#include "components/url_formatter/url_fixer.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

#if BUILDFLAG(IS_WIN)
enum class ArgType { kFile, kProtocol, kInvalid };

// Returns the dir enum defined in base/base_paths_win.h that corresponds to the
// path of `shortcut_path` if any, nullopt if no match found.
std::optional<int> GetShortcutLocation(const std::wstring& shortcut_path) {
  // The windows quick launch path is not localized.
  const std::u16string shortcut(
      base::i18n::ToLower(base::AsStringPiece16(shortcut_path)));
  if (shortcut.find(u"\\quick launch\\") != std::u16string_view::npos) {
    return base::DIR_TASKBAR_PINS;
  }

  // Check the common shortcut locations.
  constexpr int kPathKeys[] = {base::DIR_COMMON_START_MENU,
                               base::DIR_START_MENU, base::DIR_COMMON_DESKTOP,
                               base::DIR_USER_DESKTOP};
  base::FilePath candidate;
  for (int path_key : kPathKeys) {
    if (base::PathService::Get(path_key, &candidate) &&
        base::StartsWith(
            shortcut,
            base::i18n::ToLower(base::AsStringPiece16(candidate.value())),
            base::CompareCase::SENSITIVE)) {
      return path_key;
    }
  }
  return std::nullopt;
}

// Returns kFile if `arg` is a file, kProtocol if it's a protocol, and
// kInvalid if it does not seem to be either.
// This should be called off the UI thread because it can cause disk access.
ArgType GetArgType(const std::wstring& arg) {
  GURL url(base::AsStringPiece16(arg));
  if (url.is_valid() && !url.SchemeIsFile())
    return ArgType::kProtocol;
  // This handles the case of "chrome.exe foo.html".
  if (!url.is_valid()) {
    url =
        url_formatter::FixupRelativeFile(base::FilePath(), base::FilePath(arg));
    if (!url.is_valid())
      return ArgType::kInvalid;
  }
  return url.SchemeIsFile() ? ArgType::kFile : ArgType::kProtocol;
}

// Gets the path of the shortcut that launched Chrome, either from the
// command line switch --shortcut_path in the case of a rendezvous to an
// existing process, or ::GetStartupInfoW.
// Returns the empty string if Chrome wasn't launched from a shortcut.
// This can be expensive and shouldn't be called from the UI thread.
std::optional<std::wstring> GetShortcutPath(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kSourceShortcut))
    return command_line.GetSwitchValueNative(switches::kSourceShortcut);
  STARTUPINFOW si = {sizeof(si)};
  GetStartupInfoW(&si);
  return si.dwFlags & STARTF_TITLEISLINKNAME
             ? std::optional<std::wstring>(si.lpTitle)
             : std::nullopt;
}

#endif  // BUIDFLAG(IS_WIN)
}  // namespace

// new LaunchMode implementation below.

namespace {

void RecordLaunchMode(const base::CommandLine command_line,
                      std::optional<LaunchMode> mode) {
  if (mode.value_or(LaunchMode::kNone) == LaunchMode::kNone) {
    return;
  }
  base::UmaHistogramEnumeration("Launch.Mode2", mode.value());
#if BUILDFLAG(IS_WIN)
  base::UmaHistogramBoolean(
      "BrowserSwitcher.ChromeLaunch.IsFromBrowserSwitcher",
      command_line.HasSwitch(switches::kFromBrowserSwitcher));
#endif  // BUILDFLAG(IS_WIN)
}

#if BUILDFLAG(IS_WIN)
// Gets LaunchMode from `command_line`, potentially using some functions that
// might be slow, e.g., involve disk access, and hence, this should not be
// used on the UI thread.
std::optional<LaunchMode> GetLaunchModeSlow(
    const base::CommandLine command_line) {
  std::optional<std::wstring> shortcut_path = GetShortcutPath(command_line);
  bool is_app_launch = command_line.HasSwitch(switches::kApp) ||
                       command_line.HasSwitch(switches::kAppId);
  if (!shortcut_path.has_value()) {
    // Not launched from a shortcut. Check if we're launched as a registered
    // file or protocol handler.
    bool single_argument_switch = command_line.HasSingleArgumentSwitch();
    // Single argument switch means we were registered with the shell as a
    // handler for a file extension or a protocol/url. Then, determine if
    // Chrome or a web app is handling the argument.
    std::vector<base::CommandLine::StringType> args = command_line.GetArgs();
    if (args.size() < 1)
      return is_app_launch ? LaunchMode::kWebAppOther : LaunchMode::kOther;
    auto arg_type = GetArgType(args[0]);
    if (!is_app_launch) {
      if (arg_type == ArgType::kFile) {
        return single_argument_switch ? LaunchMode::kFileTypeHandler
                                      : LaunchMode::kWithFile;
      }
      if (arg_type == ArgType::kProtocol) {
        return single_argument_switch ? LaunchMode::kProtocolHandler
                                      : LaunchMode::kWithUrl;
      }
    } else {
      // Should we check if single_argument_switch is present?
      if (arg_type == ArgType::kFile)
        return LaunchMode::kWebAppFileTypeHandler;
      if (arg_type == ArgType::kProtocol)
        return LaunchMode::kWebAppProtocolHandler;
    }
  } else {
    if (shortcut_path.value().empty())
      return LaunchMode::kShortcutNoName;
    std::optional<int> shortcut_location =
        GetShortcutLocation(shortcut_path.value());
    if (!shortcut_location.has_value()) {
      return is_app_launch ? LaunchMode::kWebAppShortcutUnknown
                           : LaunchMode::kShortcutUnknown;
    }
    if (!is_app_launch) {
      static constexpr auto kDirToLaunchModeMap =
          base::MakeFixedFlatMap<int, LaunchMode>({
              {base::DIR_TASKBAR_PINS, LaunchMode::kShortcutTaskbar},
              {base::DIR_COMMON_START_MENU, LaunchMode::kShortcutStartMenu},
              {base::DIR_START_MENU, LaunchMode::kShortcutStartMenu},
              {base::DIR_COMMON_DESKTOP, LaunchMode::kShortcutDesktop},
              {base::DIR_USER_DESKTOP, LaunchMode::kShortcutDesktop},
          });
      return kDirToLaunchModeMap.at(shortcut_location.value());
    }
    static constexpr auto kDirToWebAppLaunchModeMap =
        base::MakeFixedFlatMap<int, LaunchMode>({
            {base::DIR_TASKBAR_PINS, LaunchMode::kWebAppShortcutTaskbar},
            {base::DIR_COMMON_START_MENU, LaunchMode::kWebAppShortcutStartMenu},
            {base::DIR_START_MENU, LaunchMode::kWebAppShortcutStartMenu},
            {base::DIR_COMMON_DESKTOP, LaunchMode::kWebAppShortcutDesktop},
            {base::DIR_USER_DESKTOP, LaunchMode::kWebAppShortcutDesktop},
        });
    return kDirToWebAppLaunchModeMap.at(shortcut_location.value());
  }
  return LaunchMode::kOther;
}

// Computes the launch mode from the command line. If other information is
// required that is potentially expensive to get, returns std::nullopt and
// defers to `GetLaunchModeSlow`.
std::optional<LaunchMode> GetLaunchModeFast(
    const base::CommandLine& command_line) {
  // These are the switches for which there is a 1:1 mapping to a launch mode.
  static constexpr std::pair<const char*, LaunchMode> switch_to_mode[] = {
      {switches::kRestart, LaunchMode::kNone},
      {switches::kNoStartupWindow, LaunchMode::kNone},
      {switches::kUninstallAppId, LaunchMode::kNone},
      {switches::kListApps, LaunchMode::kNone},
      {switches::kInstallChromeApp, LaunchMode::kNone},
      {switches::kFromInstaller, LaunchMode::kNone},
      {switches::kUninstall, LaunchMode::kNone},
      {switches::kNotificationLaunchId, LaunchMode::kWinPlatformNotification},
  };
  for (const auto& [switch_val, mode] : switch_to_mode) {
    if (command_line.HasSwitch(switch_val))
      return mode;
  }
  return std::nullopt;
}

#elif BUILDFLAG(IS_MAC)
std::optional<LaunchMode> GetLaunchModeSlow(
    const base::CommandLine command_line) {
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

std::optional<LaunchMode> GetLaunchModeFast(
    const base::CommandLine& command_line) {
  DiskImageStatus dmg_launch_status =
      IsAppRunningFromReadOnlyDiskImage(nullptr);
  dock::ChromeInDockStatus dock_launch_status = dock::ChromeIsInTheDock();

  if (dock_launch_status == dock::ChromeInDockFailure &&
      dmg_launch_status == DiskImageStatusFailure) {
    return LaunchMode::kMacDockDMGStatusError;
  }

  if (dock_launch_status == dock::ChromeInDockFailure)
    return LaunchMode::kMacDockStatusError;

  if (dmg_launch_status == DiskImageStatusFailure)
    return LaunchMode::kMacDMGStatusError;

  bool dmg_launch = dmg_launch_status == DiskImageStatusTrue;
  bool dock_launch = dock_launch_status == dock::ChromeInDockTrue;

  if (dmg_launch && dock_launch)
    return LaunchMode::kMacDockedDMGLaunch;

  if (dmg_launch)
    return LaunchMode::kMacUndockedDMGLaunch;

  if (dock_launch)
    return LaunchMode::kMacDockedDiskLaunch;

  return LaunchMode::kMacUndockedDiskLaunch;
}
#else  //  !IS_WIN && !IS_MAC
std::optional<LaunchMode> GetLaunchModeSlow(
    const base::CommandLine command_line) {
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

std::optional<LaunchMode> GetLaunchModeFast(
    const base::CommandLine& command_line) {
  return LaunchMode::kOtherOS;
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

void ComputeAndRecordLaunchMode(const base::CommandLine& command_line) {
  ComputeLaunchMode(command_line,
                    base::BindOnce(&RecordLaunchMode, command_line));
}

// Computes the launch mode based on `command_line` and process state. Runs
// `result_callback` with the result either synchronously or asynchronously on
// the caller's sequence.
void ComputeLaunchMode(
    const base::CommandLine& command_line,
    base::OnceCallback<void(std::optional<LaunchMode>)> result_callback) {
  if (auto mode = GetLaunchModeFast(command_line); mode.has_value()) {
    std::move(result_callback).Run(mode);
    return;
  }
  auto split = base::SplitOnceCallback(std::move(result_callback));
  if (!base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&GetLaunchModeSlow, command_line),
          std::move(split.first))) {
    std::move(split.second).Run(std::nullopt);
  }
}

base::OnceCallback<void(std::optional<LaunchMode>)>
GetRecordLaunchModeForTesting() {
  return base::BindOnce(&RecordLaunchMode,
                        base::CommandLine(base::CommandLine::NO_PROGRAM));
}
