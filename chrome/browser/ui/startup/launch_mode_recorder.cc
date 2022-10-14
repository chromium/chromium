// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/launch_mode_recorder.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/mac/dock.h"
#include "chrome/browser/mac/install_from_dmg.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/files/file_path.h"
#include "base/win/startup_information.h"
#include "chrome/installer/util/taskbar_util.h"
#endif

namespace {

// Returns a LaunchMode value if one can be determined with low overhead, or
// kToBeDecided if a call to GetLaunchModeSlow is required.
OldLaunchMode GetLaunchModeFast();

// Returns a LaunchMode value; may require a bit of extra work. This will be
// called on a background thread outside of the critical startup path.
OldLaunchMode GetLaunchModeSlow();

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

OldLaunchMode GetLaunchModeFast() {
  auto shortcut_path = GetShortcutPath();
  if (!shortcut_path)
    return OldLaunchMode::kOther;
  if (!shortcut_path.value())
    return OldLaunchMode::kShortcutNoName;
  return OldLaunchMode::kToBeDecided;
}

OldLaunchMode GetLaunchModeSlow() {
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
OldLaunchMode GetLaunchModeFast() {
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

OldLaunchMode GetLaunchModeSlow() {
  NOTREACHED();
  return OldLaunchMode::kToBeDecided;
}
#else                    // BUILDFLAG(IS_WIN)
// TODO(cpu): Port to other platforms.
OldLaunchMode GetLaunchModeFast() {
  return OldLaunchMode::kOtherOS;
}

OldLaunchMode GetLaunchModeSlow() {
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
      (mode = GetLaunchModeFast()) == OldLaunchMode::kToBeDecided) {
    // The mode couldn't be determined with a fast path. Perform a more
    // expensive evaluation out of the critical startup path.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&RecordOldLaunchMode, GetLaunchModeSlow()));
  } else {
    RecordOldLaunchMode(mode);
  }
}

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
