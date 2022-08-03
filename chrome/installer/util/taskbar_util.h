// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common Windows Taskbar manipulation functions that can be used by the Windows
// installer as well as Chrome.

#ifndef CHROME_INSTALLER_UTIL_TASKBAR_UTIL_H_
#define CHROME_INSTALLER_UTIL_TASKBAR_UTIL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

// Pin to taskbar is supported on Windows 7, 8 and Win10RS5+. Returns
// true on those platforms.
bool CanPinShortcutToTaskbar();

// Pins a shortcut to the taskbar on Windows 7, 8 and Win10RS5+ . `shortcut`
// file must already exist and be a shortcut that points to an executable.The
// app id of the shortcut is used to group windows and must be set correctly.
bool PinShortcutToTaskbar(const base::FilePath& shortcut);

// Unpins a shortcut from the Windows 7+ taskbar. `shortcut` must exist
// and already be pinned to the taskbar. The app id of the shortcut is used as
// the identifier for the taskbar item to remove and must be set correctly.
bool UnpinShortcutFromTaskbar(const base::FilePath& shortcut);

// Returns true if `shortcut` is pinned, false if not, and nullopt if
// IPinnedList3 is not supported (e.g., pre WIN10_RS5). Do not to call
// this on the Browser UI thread since it calls CoCreateInstance, which can
// cause jank.
absl::optional<bool> IsShortcutPinnedToTaskbar(const base::FilePath& shortcut);

#endif  // CHROME_INSTALLER_UTIL_TASKBAR_UTIL_H_
