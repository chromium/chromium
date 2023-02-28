// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FRESNEL_PREF_NAMES_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FRESNEL_PREF_NAMES_H_

#include "base/component_export.h"

namespace ash::prefs {

// ---------------------------------------------------------------------------
// Prefs related to ChromeOS device active pings.
// ---------------------------------------------------------------------------

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
extern const char kDeviceActiveLastKnownDailyPingTimestamp[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
extern const char kDeviceActiveLastKnown28DayActivePingTimestamp[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
extern const char kDeviceActiveChurnCohortMonthlyPingTimestamp[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
extern const char kDeviceActiveChurnObservationMonthlyPingTimestamp[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
extern const char kDeviceActiveLastKnownChurnActiveStatus[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
extern const char kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
extern const char kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
extern const char kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2[];

}  // namespace ash::prefs

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FRESNEL_PREF_NAMES_H_
