// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/message_center/metrics_utils.h"

#include "base/metrics/histogram_functions.h"

namespace ash::metrics_utils {

void LogArcNotificationIsCustomNotification(bool is_custom_notification) {
  base::UmaHistogramBoolean("Arc.Notifications.IsCustomNotification",
                            is_custom_notification);
}

}  // namespace ash::metrics_utils
