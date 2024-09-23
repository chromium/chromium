// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_PREF_NAMES_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_PREF_NAMES_H_

namespace ash::prefs {

// Determines whether Kiosk Vision telemetry is enabled.
inline constexpr char kKioskVisionTelemetryEnabled[] =
    "kiosk_vision_telemetry_enabled";

// Determines the reporting frequency of Kiosk Vision's telemetry.
inline constexpr char kKioskVisionTelemetryFrequency[] =
    "kiosk_vision_telemetry_frequency";

}  // namespace ash::prefs

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_PREF_NAMES_H_
