// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_CONSTANTS_H_

#include "base/time/time.h"

namespace ash::boca {
// Interval for countdown notifications.
inline constexpr base::TimeDelta kSpotlightNotificationCountdownInterval =
    base::Seconds(1);

// Duration for how long the countdown is shown for.
inline constexpr base::TimeDelta kSpotlightNotificationDuration =
    base::Seconds(3);

// Notifier id for Spotlight notifications.
inline constexpr char kSpotlightNotifierId[] = "boca.spotlight";

// Notification id for the notification shown before starting a
// spotlight_session.
inline constexpr char kSpotlightStartedNotificationId[] =
    "boca.spotlight.starting_notification";
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_CONSTANTS_H_
