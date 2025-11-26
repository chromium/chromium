// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_PORTAL_CONSTANTS_H_
#define COMPONENTS_DBUS_XDG_PORTAL_CONSTANTS_H_

namespace dbus_xdg {

inline constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
inline constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/desktop";
inline constexpr char kRegistryInterface[] =
    "org.freedesktop.host.portal.Registry";
inline constexpr char kMethodRegister[] = "Register";

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_PORTAL_CONSTANTS_H_
