// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"

namespace ash::prefs {

// Timestamp of last known daily ping to Fresnel.
const char kDeviceActiveLastKnownDailyPingTimestamp[] =
    "device_active.last_known_daily_ping_timestamp";

// Timestamp of last known 28 day active ping to Fresnel.
const char kDeviceActiveLastKnown28DayActivePingTimestamp[] =
    "device_active.last_known_28day_active_ping_timestamp";

// Timestamp of last known monthly churn cohort ping to Fresnel.
const char kDeviceActiveChurnCohortMonthlyPingTimestamp[] =
    "device_active.last_known_churn_cohort_monthly_ping_timestamp";

}  // namespace ash::prefs
