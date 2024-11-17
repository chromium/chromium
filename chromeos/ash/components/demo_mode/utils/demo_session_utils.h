// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DEMO_SESSION_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DEMO_SESSION_UTILS_H_

#include "base/component_export.h"

class PrefRegistrySimple;

namespace ash::demo_mode {

// The list of countries that Demo Mode supports, ie the countries we have
// created OUs and admin users for in the admin console.
// Sorted by country code except US is first.
inline constexpr char kSupportedCountries[][3] = {
    "US", "AT", "AU", "BE", "BR", "CA", "DE", "DK", "ES",
    "FI", "FR", "GB", "IE", "IN", "IT", "JP", "LU", "MX",
    "NL", "NO", "NZ", "PL", "PT", "SE", "ZA"};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
// Whether the device is set up to run demo sessions.
bool IsDeviceInDemoMode();

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace ash::demo_mode

#endif  // CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DEMO_SESSION_UTILS_H_
