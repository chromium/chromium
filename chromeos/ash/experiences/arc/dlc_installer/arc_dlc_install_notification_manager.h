// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_INSTALL_NOTIFICATION_MANAGER_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_INSTALL_NOTIFICATION_MANAGER_H_

#include <string_view>

namespace arc {

// Manages notifications for ARC DLC installation processes.
namespace arc_dlc_install_notification_manager {

inline constexpr std::string_view kArcVmPreloadStartedId =
    "arc_dlc_install/started";
inline constexpr std::string_view kArcVmPreloadSucceededId =
    "arc_dlc_install/succeeded";
inline constexpr std::string_view kArcVmPreloadFailedId =
    "arc_dlc_install/failed";

// Represents the type of notification to be displayed.
enum class NotificationType {
  kArcVmPreloadStarted,
  kArcVmPreloadSucceeded,
  kArcVmPreloadFailed
};

// Displays a notification of the specified type.
void Show(NotificationType notification_type);

}  // namespace arc_dlc_install_notification_manager

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_INSTALL_NOTIFICATION_MANAGER_H_
