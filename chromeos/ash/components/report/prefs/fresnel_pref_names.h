// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_PREFS_FRESNEL_PREF_NAMES_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_PREFS_FRESNEL_PREF_NAMES_H_

#include "base/component_export.h"

namespace ash::report::prefs {

// ---------------------------------------------------------------------------
// Prefs related to ChromeOS device active pings.
// ---------------------------------------------------------------------------

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
extern const char kDeviceActiveLastKnown1DayActivePingTimestamp[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
extern const char kDeviceActiveLastKnown28DayActivePingTimestamp[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
extern const char kDeviceActiveChurnCohortMonthlyPingTimestamp[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
extern const char kDeviceActiveChurnObservationMonthlyPingTimestamp[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
extern const char kDeviceActiveLastKnownChurnActiveStatus[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
extern const char kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
extern const char kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
extern const char kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2[];
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
extern const char kDeviceActive28DayActivePingCache[];

}  // namespace ash::report::prefs

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_PREFS_FRESNEL_PREF_NAMES_H_
