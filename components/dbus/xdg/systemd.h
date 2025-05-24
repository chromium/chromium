// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_SYSTEMD_H_
#define COMPONENTS_DBUS_XDG_SYSTEMD_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace dbus {
class Bus;
}

namespace dbus_xdg {

enum class COMPONENT_EXPORT(COMPONENTS_DBUS) SystemdUnitStatus {
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
// get the terminal name. This function caches it's results and may be called
// more than once, but must be called on the same sequence.
COMPONENT_EXPORT(COMPONENTS_DBUS)
void SetSystemdScopeUnitNameForXdgPortal(dbus::Bus* bus,
                                         SystemdUnitCallback callback);

COMPONENT_EXPORT(COMPONENTS_DBUS)
void ResetCachedStateForTesting();

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_SYSTEMD_H_
