// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_PORTAL_H_
#define COMPONENTS_DBUS_XDG_PORTAL_H_

#include <cstdint>
#include <string_view>

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

using PortalSetupCallback = base::OnceCallback<void(uint32_t version)>;

// Initializes the XDG desktop portal by setting the systemd scope unit name,
// ensuring the portal service is started, and registering the application,
// then queries the version of `interface_name` (e.g.
// "org.freedesktop.portal.FileChooser"). The setup is only done once, and
// interface versions are cached, so this may be called more than once.
// Runs `callback` with the version of `interface_name` if the portal and that
// interface are available, or 0 otherwise.
//
// Callers must use this before calling any portal interface, since the app
// must be registered before the portal is first used.
COMPONENT_EXPORT(COMPONENTS_DBUS)
void RequestXdgDesktopPortal(dbus::Bus* bus,
                             std::string_view interface_name,
                             PortalSetupCallback callback);

// Resets the portal setup state to `state` and clears cached interface
// versions. When `state` is `kSuccess`, every interface reports version
// `kDefaultPortalVersionForTesting` unless overridden with
// SetPortalInterfaceVersionForTesting().
inline constexpr uint32_t kDefaultPortalVersionForTesting = 3;
COMPONENT_EXPORT(COMPONENTS_DBUS)
void SetPortalStateForTesting(PortalRegistrarState state);

// Overrides the version reported for `interface_name`. A version of 0 makes
// the interface unavailable. Must be called after SetPortalStateForTesting().
COMPONENT_EXPORT(COMPONENTS_DBUS)
void SetPortalInterfaceVersionForTesting(std::string_view interface_name,
                                         uint32_t version);

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_PORTAL_H_
