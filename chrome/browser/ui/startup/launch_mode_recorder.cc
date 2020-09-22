// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/mac/dock.h"
#include "chrome/browser/mac/install_from_dmg.h"
#endif

#if defined(OS_WIN)
#include "base/files/file_path.h"
#include "base/win/startup_information.h"
#endif

namespace {

// Returns a LaunchMode value if one can be determined with low overhead, or
// kToBeDecided if a call to GetLaunchModeSlow is required.
LaunchMode GetLaunchModeFast();

// Returns a LaunchMode value; may require a bit of extra work. This will be
// called on a background thread outside of the critical startup path.
LaunchMode GetLaunchModeSlow();

#if defined(OS_WIN)
// Returns the path to the shortcut from which Chrome was launched, or null if
// not launched via a shortcut.
base::Optional<const wchar_t*> GetShortcutPath() {
  STARTUPINFOW si = {sizeof(si)};
  GetStartupInfoW(&si);
  if (!(si.dwFlags & STARTF_TITLEISLINKNAME))
    return base::nullopt;
  return base::Optional<const wchar_t*>(si.lpTitle);
}

LaunchMode GetLaunchModeFast() {
  auto shortcut_path = GetShortcutPath();
  if (!shortcut_path)
    return LaunchMode::kOther;
  if (!shortcut_path.value())
    return LaunchMode::kShortcutNoName;
  return LaunchMode::kToBeDecided;
}

LaunchMode GetLaunchModeSlow() {
  auto shortcut_path = GetShortcutPath();
  DCHECK(shortcut_path);
  DCHECK(shortcut_path.value());

  const base::string16 shortcut(base::i18n::ToLower(shortcut_path.value()));

  // The windows quick launch path is not localized.
  if (shortcut.find(L"\\quick launch\\") != base::StringPiece16::npos)
    return LaunchMode::kShortcutTaskbar;

  // Check the common shortcut locations.
  static constexpr struct {
    int path_key;
    LaunchMode launch_mode;
  } kPathKeysAndModes[] = {
      {base::DIR_COMMON_START_MENU, LaunchMode::kShortcutStartMenu},
      {base::DIR_START_MENU, LaunchMode::kShortcutStartMenu},
      {base::DIR_COMMON_DESKTOP, LaunchMode::kShortcutDesktop},
      {base::DIR_USER_DESKTOP, LaunchMode::kShortcutDesktop},
  };
  base::FilePath candidate;
  for (const auto& item : kPathKeysAndModes) {
    if (base::PathService::Get(item.path_key, &candidate) &&
        base::StartsWith(shortcut, base::i18n::ToLower(candidate.value()),
                         base::CompareCase::SENSITIVE)) {
      return item.launch_mode;
    }
  }

  return LaunchMode::kShortcutUnknown;
}
#elif defined(OS_MAC)  // defined(OS_WIN)
LaunchMode GetLaunchModeFast() {
  DiskImageStatus dmg_launch_status =
      IsAppRunningFromReadOnlyDiskImage(nullptr);
  dock::ChromeInDockStatus dock_launch_status = dock::ChromeIsInTheDock();

  if (dock_launch_status == dock::ChromeInDockFailure &&
      dmg_launch_status == DiskImageStatusFailure)
    return LaunchMode::kMacDockDMGStatusError;

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

LaunchMode GetLaunchModeSlow() {
  NOTREACHED();
  return LaunchMode::kToBeDecided;
}
#else                  // defined(OS_WIN)
// TODO(cpu): Port to other platforms.
LaunchMode GetLaunchModeFast() {
  return LaunchMode::kOtherOS;
}

LaunchMode GetLaunchModeSlow() {
  NOTREACHED();
  return LaunchMode::kOtherOS;
}
#endif                 // defined(OS_WIN)

// Log in a histogram the frequency of launching by the different methods. See
// LaunchMode enum for the actual values of the buckets.
void RecordLaunchModeHistogram(LaunchMode mode) {
  static constexpr char kLaunchModesHistogram[] = "Launch.Modes";
  if (mode == LaunchMode::kToBeDecided &&
      (mode = GetLaunchModeFast()) == LaunchMode::kToBeDecided) {
    // The mode couldn't be determined with a fast path. Perform a more
    // expensive evaluation out of the critical startup path.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce([]() {
          base::UmaHistogramSparse(kLaunchModesHistogram,
                                   static_cast<int>(GetLaunchModeSlow()));
        }));
  } else {
    base::UmaHistogramSparse(kLaunchModesHistogram, static_cast<int>(mode));
  }
}

}  // namespace

LaunchModeRecorder::LaunchModeRecorder() = default;

LaunchModeRecorder::~LaunchModeRecorder() {
  if (mode_.has_value())
    RecordLaunchModeHistogram(mode_.value());
}

void LaunchModeRecorder::SetLaunchMode(LaunchMode mode) {
  if (!mode_.has_value())
    mode_ = mode;
}
