// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_NOTIFICATION_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_NOTIFICATION_CONSTANTS_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/time/time.h"

namespace ash::boca {

// Interval for countdown notifications.
inline constexpr base::TimeDelta kOnTaskNotificationCountdownInterval =
    base::Seconds(1);

// Notifier id for OnTask notifications.
inline constexpr char kOnTaskNotifierId[] = "boca.on_task";

// Notification id for the notification shown before entering locked fullscreen.
inline constexpr char kOnTaskEnterLockedModeNotificationId[] =
    "boca.on_task.enter_locked_mode_notification";

// Notification id for the notification shown after the session has ended.
inline constexpr char kOnTaskSessionEndNotificationId[] =
    "boca.on_task.session_end_notification";

// Toast id for the toast shown after a URL is blocked.
inline constexpr char kOnTaskUrlBlockedToastId[] = "OnTaskURLBlockedToast";

// Notification id for the notification shown when new content is added to a
// bundle.
inline constexpr char kOnTaskBundleContentAddedNotificationId[] =
    "boca.on_task.bundle_content_added_notification";

// Notification id for the notification shown when content is removed from a
// bundle.
inline constexpr char kOnTaskBundleContentRemovedNotificationId[] =
    "boca.on_task.bundle_content_removed_notification";

// Returns the allowlisted notifications for OnTask in locked mode.
base::flat_set<std::string> GetAllowlistedNotificationIdsForLockedMode();

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_NOTIFICATION_CONSTANTS_H_
