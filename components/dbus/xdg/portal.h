// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_PORTAL_H_
#define COMPONENTS_DBUS_XDG_PORTAL_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace dbus {
class Bus;
}

namespace dbus_xdg {

enum class PortalRegistrarState {
  kIdle,
  kInitializing,
  kSuccess,
  kFailed,
};

using PortalSetupCallback = base::OnceCallback<void(bool success)>;

// Initializes the XDG desktop portal by setting the systemd scope unit name,
// ensuring the portal service is started, and registering the application.
// This function caches its results and may be called more than once.
// `callback` is run with true iff the portal is available.
COMPONENT_EXPORT(COMPONENTS_DBUS)
void RequestXdgDesktopPortal(dbus::Bus* bus, PortalSetupCallback callback);

COMPONENT_EXPORT(COMPONENTS_DBUS)
void SetPortalStateForTesting(PortalRegistrarState state);

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_PORTAL_H_
