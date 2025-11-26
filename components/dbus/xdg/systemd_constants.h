// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_XDG_SYSTEMD_CONSTANTS_H_
#define COMPONENTS_DBUS_XDG_SYSTEMD_CONSTANTS_H_

namespace dbus_xdg {

inline constexpr char kServiceNameSystemd[] = "org.freedesktop.systemd1";
inline constexpr char kObjectPathSystemd[] = "/org/freedesktop/systemd1";
inline constexpr char kInterfaceSystemdManager[] =
    "org.freedesktop.systemd1.Manager";
inline constexpr char kInterfaceSystemdUnit[] = "org.freedesktop.systemd1.Unit";
inline constexpr char kMethodStartTransientUnit[] = "StartTransientUnit";
inline constexpr char kMethodGetUnit[] = "GetUnit";
inline constexpr char kSystemdActiveStateProp[] = "ActiveState";
inline constexpr char kSystemdStateActive[] = "active";
inline constexpr char kSystemdStateInactive[] = "inactive";

}  // namespace dbus_xdg

#endif  // COMPONENTS_DBUS_XDG_SYSTEMD_CONSTANTS_H_
