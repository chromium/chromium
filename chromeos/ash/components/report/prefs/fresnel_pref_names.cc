// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/prefs/fresnel_pref_names.h"

namespace ash::report::prefs {

// Pacific adjusted timestamp of last known 1-day-active ping to Fresnel, stored
// as UTC.
const char kDeviceActiveLastKnown1DayActivePingTimestamp[] =
    "device_active.last_known_daily_ping_timestamp";

// Pacific adjusted timestamp of last known 28-day-active ping to Fresnel,
// stored as UTC.
const char kDeviceActiveLastKnown28DayActivePingTimestamp[] =
    "device_active.last_known_28day_active_ping_timestamp";

// Pacific adjusted timestamp of last known monthly churn cohort ping to
// Fresnel, stored as UTC.
const char kDeviceActiveChurnCohortMonthlyPingTimestamp[] =
    "device_active.last_known_churn_cohort_monthly_ping_timestamp";

// Pacific adjusted timestamp of last known monthly churn observation ping to
// Fresnel, stored as UTC.
const char kDeviceActiveChurnObservationMonthlyPingTimestamp[] =
    "device_active.last_known_churn_observation_monthly_ping_timestamp";

// Int representing the 28 bit Active Status used for the churn use case.
// The first 10 bits represent number months from 01/01/2000 to current month.
// Remaining 18 bits represents past 18 months when device was active from
// current month.
const char kDeviceActiveLastKnownChurnActiveStatus[] =
    "device_active.last_known_churn_active_status";

// The observation status for the past three periods from the current date.
// The current date is determined based on the first 10 bits of
// |kDeviceActiveLastKnownChurnActiveStatus|.
const char kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0[] =
    "device_active.last_known_is_active_current_period_minus_0";
const char kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1[] =
    "device_active.last_known_is_active_current_period_minus_1";
const char kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2[] =
    "device_active.last_known_is_active_current_period_minus_2";

// Dictionary storing 28 day rolling history of device active pings.
const char kDeviceActive28DayActivePingCache[] =
    "device_active.28day_active_ping_cache";

}  // namespace ash::report::prefs
