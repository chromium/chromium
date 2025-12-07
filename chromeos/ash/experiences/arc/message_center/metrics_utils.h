// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_MESSAGE_CENTER_METRICS_UTILS_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_MESSAGE_CENTER_METRICS_UTILS_H_

#include "chromeos/ash/experiences/arc/mojom/notifications.mojom.h"

namespace ash::metrics_utils {

// Logs if the notification is custom notification.
void LogArcNotificationIsCustomNotification(bool is_custom_notification);

}  // namespace ash::metrics_utils

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_MESSAGE_CENTER_METRICS_UTILS_H_
