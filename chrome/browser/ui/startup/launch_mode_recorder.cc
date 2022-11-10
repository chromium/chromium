// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/launch_mode_recorder.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
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

// Returns an OldLaunchMode value if one can be determined with low overhead, or
// kToBeDecided if a call to GetOldLaunchModeSlow is required.
OldLaunchMode GetOldLaunchModeFast();

// Returns an OldLaunchMode value; may require a bit of extra work. This will be
// called on a background thread outside of the critical startup path.
OldLaunchMode GetOldLaunchModeSlow();

#if BUILDFLAG(IS_WIN)
// Returns the path to the shortcut from which Chrome was launched, or null if
// not launched via a shortcut.
absl::optional<const wchar_t*> GetShortcutPath() {
  STARTUPINFOW si = {sizeof(si)};
  GetStartupInfoW(&si);
  if (!(si.dwFlags & STARTF_TITLEISLINKNAME))
    return absl::nullopt;
  return absl::optional<const wchar_t*>(si.lpTitle);
}

OldLaunchMode GetOldLaunchModeFast() {
  auto shortcut_path = GetShortcutPath();
  if (!shortcut_path)
    return OldLaunchMode::kOther;
  if (!shortcut_path.value())
    return OldLaunchMode::kShortcutNoName;
  return OldLaunchMode::kToBeDecided;
}

OldLaunchMode GetOldLaunchModeSlow() {
  auto shortcut_path = GetShortcutPath();
  DCHECK(shortcut_path);
  DCHECK(shortcut_path.value());

  const std::u16string shortcut(
      base::i18n::ToLower(base::WideToUTF16(shortcut_path.value())));

  // The windows quick launch path is not localized.
  if (shortcut.find(u"\\quick launch\\") != base::StringPiece16::npos)
    return OldLaunchMode::kShortcutTaskbar;

  // Check the common shortcut locations.
  static constexpr struct {
    int path_key;
    OldLaunchMode launch_mode;
  } kPathKeysAndModes[] = {
      {base::DIR_COMMON_START_MENU, OldLaunchMode::kShortcutStartMenu},
      {base::DIR_START_MENU, OldLaunchMode::kShortcutStartMenu},
      {base::DIR_COMMON_DESKTOP, OldLaunchMode::kShortcutDesktop},
      {base::DIR_USER_DESKTOP, OldLaunchMode::kShortcutDesktop},
  };
  base::FilePath candidate;
  for (const auto& item : kPathKeysAndModes) {
    if (base::PathService::Get(item.path_key, &candidate) &&
        base::StartsWith(shortcut,
                         base::i18n::ToLower(candidate.AsUTF16Unsafe()),
                         base::CompareCase::SENSITIVE)) {
      return item.launch_mode;
    }
  }

  return OldLaunchMode::kShortcutUnknown;
}
#elif BUILDFLAG(IS_MAC)  // BUILDFLAG(IS_WIN)
OldLaunchMode GetOldLaunchModeFast() {
  DiskImageStatus dmg_launch_status =
      IsAppRunningFromReadOnlyDiskImage(nullptr);
  dock::ChromeInDockStatus dock_launch_status = dock::ChromeIsInTheDock();

  if (dock_launch_status == dock::ChromeInDockFailure &&
      dmg_launch_status == DiskImageStatusFailure)
    return OldLaunchMode::kMacDockDMGStatusError;

  if (dock_launch_status == dock::ChromeInDockFailure)
    return OldLaunchMode::kMacDockStatusError;

  if (dmg_launch_status == DiskImageStatusFailure)
    return OldLaunchMode::kMacDMGStatusError;

  bool dmg_launch = dmg_launch_status == DiskImageStatusTrue;
  bool dock_launch = dock_launch_status == dock::ChromeInDockTrue;

  if (dmg_launch && dock_launch)
    return OldLaunchMode::kMacDockedDMGLaunch;

  if (dmg_launch)
    return OldLaunchMode::kMacUndockedDMGLaunch;

  if (dock_launch)
    return OldLaunchMode::kMacDockedDiskLaunch;

  return OldLaunchMode::kMacUndockedDiskLaunch;
}

OldLaunchMode GetOldLaunchModeSlow() {
  NOTREACHED();
  return OldLaunchMode::kToBeDecided;
}
#else                    // BUILDFLAG(IS_WIN)
OldLaunchMode GetOldLaunchModeFast() {
  return OldLaunchMode::kOtherOS;
}

OldLaunchMode GetOldLaunchModeSlow() {
  NOTREACHED();
  return OldLaunchMode::kOtherOS;
}
#endif                   // BUILDFLAG(IS_WIN)

void RecordOldLaunchMode(OldLaunchMode mode) {
  base::UmaHistogramEnumeration("Launch.Modes", mode);
#if BUILDFLAG(IS_WIN)
  if (mode == OldLaunchMode::kShortcutTaskbar) {
    absl::optional<bool> installer_pinned = GetInstallerPinnedChromeToTaskbar();
    if (installer_pinned.has_value()) {
      base::UmaHistogramBoolean("Windows.Launch.TaskbarInstallerPinned",
                                installer_pinned.value());
    }
  }
#endif  // BUILDFLAG(IS_WIN)
}

// Log in a histogram the frequency of launching by the different methods. See
// LaunchMode enum for the actual values of the buckets.
void RecordOldLaunchModeHistogram(OldLaunchMode mode) {
  if (mode == OldLaunchMode::kToBeDecided &&
      (mode = GetOldLaunchModeFast()) == OldLaunchMode::kToBeDecided) {
    // The mode couldn't be determined with a fast path. Perform a more
    // expensive evaluation out of the critical startup path.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&RecordOldLaunchMode, GetOldLaunchModeSlow()));
  } else {
    RecordOldLaunchMode(mode);
  }
}

#if BUILDFLAG(IS_WIN)

enum class ArgType { kFile, kProtocol, kInvalid };

// Returns the dir enum defined in base/base_paths_win.h that corresponds to the
// path of `shortcut_path` if any, nullopt if no match found.
absl::optional<int> GetShortcutLocation(const std::wstring& shortcut_path) {
  // The windows quick launch path is not localized.
  const std::u16string shortcut(
      base::i18n::ToLower(base::AsStringPiece16(shortcut_path)));
  if (shortcut.find(u"\\quick launch\\") != base::StringPiece16::npos)
    return base::DIR_TASKBAR_PINS;

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
  return absl::nullopt;
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
absl::optional<std::wstring> GetShortcutPath(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kSourceShortcut))
    return command_line.GetSwitchValueNative(switches::kSourceShortcut);
  STARTUPINFOW si = {sizeof(si)};
  GetStartupInfoW(&si);
  return si.dwFlags & STARTF_TITLEISLINKNAME
             ? absl::optional<std::wstring>(si.lpTitle)
             : absl::nullopt;
}

#endif  // BUIDFLAG(IS_WIN)
}  // namespace

OldLaunchModeRecorder::OldLaunchModeRecorder() = default;

OldLaunchModeRecorder::~OldLaunchModeRecorder() {
  if (mode_.has_value())
    RecordOldLaunchModeHistogram(mode_.value());
}

void OldLaunchModeRecorder::SetLaunchMode(OldLaunchMode mode) {
  if (!mode_.has_value())
    mode_ = mode;
}

// new LaunchMode implementation below.

namespace {

void RecordLaunchMode(absl::optional<LaunchMode> mode) {
  if (mode.value_or(LaunchMode::kNone) == LaunchMode::kNone)
    return;
  base::UmaHistogramEnumeration("Launch.Mode2", mode.value());
#if BUILDFLAG(IS_WIN)
  if (mode == LaunchMode::kShortcutTaskbar) {
    absl::optional<bool> installer_pinned = GetInstallerPinnedChromeToTaskbar();
    if (installer_pinned.has_value()) {
      base::UmaHistogramBoolean("Windows.Launch.TaskbarInstallerPinned",
                                installer_pinned.value());
    }
  }
#endif  // BUILDFLAG(IS_WIN)
}

#if BUILDFLAG(IS_WIN)
// Gets LaunchMode from `command_line`, potentially using some functions that
// might be slow, e.g., involve disk access, and hence, this should not be
// used on the UI thread.
absl::optional<LaunchMode> GetLaunchModeSlow(
    const base::CommandLine command_line) {
  absl::optional<std::wstring> shortcut_path = GetShortcutPath(command_line);
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
    absl::optional<int> shortcut_location =
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
// required that is potentially expensive to get, returns absl::nullopt and
// defers to `GetLaunchModeSlow`.
absl::optional<LaunchMode> GetLaunchModeFast(
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
      {switches::kTryChromeAgain, LaunchMode::kUserExperiment},
      {switches::kNotificationLaunchId, LaunchMode::kWinPlatformNotification},
  };
  for (const auto& [switch_val, mode] : switch_to_mode) {
    if (command_line.HasSwitch(switch_val))
      return mode;
  }
  return absl::nullopt;
}

#elif BUILDFLAG(IS_MAC)
absl::optional<LaunchMode> GetLaunchModeSlow(
    const base::CommandLine command_line) {
  NOTREACHED();
  return absl::nullopt;
}

absl::optional<LaunchMode> GetLaunchModeFast(
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
absl::optional<LaunchMode> GetLaunchModeSlow(
    const base::CommandLine command_line) {
  NOTREACHED();
  return absl::nullopt;
}

absl::optional<LaunchMode> GetLaunchModeFast(
    const base::CommandLine& command_line) {
  return LaunchMode::kOtherOS;
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

void ComputeAndRecordLaunchMode(const base::CommandLine& command_line) {
  ComputeLaunchMode(command_line, base::BindOnce(&RecordLaunchMode));
}

// Computes the launch mode based on `command_line` and process state. Runs
// `result_callback` with the result either synchronously or asynchronously on
// the caller's sequence.
void ComputeLaunchMode(
    const base::CommandLine& command_line,
    base::OnceCallback<void(absl::optional<LaunchMode>)> result_callback) {
  if (auto mode = GetLaunchModeFast(command_line); mode.has_value()) {
    std::move(result_callback).Run(mode);
    return;
  }
  auto split = base::SplitOnceCallback(std::move(result_callback));
  if (!base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&GetLaunchModeSlow, command_line),
          std::move(split.first))) {
    std::move(split.second).Run(absl::nullopt);
  }
}

base::OnceCallback<void(absl::optional<LaunchMode>)>
GetRecordLaunchModeForTesting() {
  return base::BindOnce(&RecordLaunchMode);
}
