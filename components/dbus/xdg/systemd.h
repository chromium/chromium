// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_SYSTEMD_H_
#define COMPONENTS_DBUS_XDG_SYSTEMD_H_

#include <sys/types.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}

namespace dbus {
class Bus;
}

namespace dbus_xdg::internal {

// Escapes a string for usage in a systemd unit name.
std::string EscapeSystemdUnitName(std::string_view name);

// Returns the PIDs of all descendants of `root` (found by scanning `proc_dir`,
// normally "/proc") whose cgroup differs from the cgroup of `root`. Parents
// are listed before their children.
std::vector<pid_t> GetDescendantsOutsideCgroup(const base::FilePath& proc_dir,
                                               pid_t root);

enum class SystemdUnitStatus {
  kUnitStarted,
  kUnitNotNecessary,
  kInvalidPid,
  kNoSystemdService,
  kFailedToStart,
};

using SystemdUnitCallback = base::OnceCallback<void(SystemdUnitStatus)>;

// xdg-desktop-portal obtains the application name from the systemd unit name
// (except in Flatpak or Snap environments). The name is passed as an ID to
// backends like Secret and GlobalShortcuts, so it's effectively required to set
// a systemd unit name for those, otherwise it will use an empty string as the
// ID which is problematic because it may collide with other apps that don't run
// under a unit. Also, if using a desktop environment like GNOME or KDE, a
// systemd scope gets created when spawning via the application launcher. Child
// processes also inherit the scope, so if the app launcher creates a terminal
// which the user launches the browser with, then the browser will incorrectly
// get the terminal name. This function is internal and is only used for portal
// initialization in RequestXdgDesktopPortal.
//
// Child processes that were spawned before the scope was created (e.g. the
// zygotes and the GPU process) are moved into the new scope too, so that
// stopping the original scope (e.g. when the terminal closes or the session
// ends) does not kill them while leaving the browser process running.
// `proc_dir` is the procfs root (normally "/proc") scanned to find them.
void SetSystemdScopeUnitNameForXdgPortal(dbus::Bus* bus,
                                         const base::FilePath& proc_dir,
                                         SystemdUnitCallback callback);

}  // namespace dbus_xdg::internal

#endif  // COMPONENTS_DBUS_XDG_SYSTEMD_H_
