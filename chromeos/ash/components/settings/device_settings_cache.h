// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SETTINGS_DEVICE_SETTINGS_CACHE_H_
#define CHROMEOS_ASH_COMPONENTS_SETTINGS_DEVICE_SETTINGS_CACHE_H_

#include <string>

#include "base/component_export.h"
#include "build/build_config.h"

static_assert(BUILDFLAG(IS_CHROMEOS), "For ChromeOS only");

namespace enterprise_management {
class PolicyData;
}

class PrefService;
class PrefRegistrySimple;

// There is need (metrics at OOBE stage) to store settings (that normally would
// go into DeviceSettings storage) before owner has been assigned (hence no key
// is available). This set of functions serves as a transient storage in that
// case.
namespace ash::device_settings_cache {

namespace prefs {

// Dictionary for transient storage of settings that should go into device
// settings storage before owner has been assigned.
inline constexpr char kDeviceSettingsCache[] = "signed_settings_cache";

}  // namespace prefs

// Registers required pref section.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SETTINGS)
void RegisterPrefs(PrefRegistrySimple* registry);

// Stores a new policy blob inside the cache stored in |local_state|.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SETTINGS)
bool Store(const enterprise_management::PolicyData& policy,
           PrefService* local_state);

// Retrieves the policy blob from the cache stored in |local_state|.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SETTINGS)
bool Retrieve(enterprise_management::PolicyData* policy,
              PrefService* local_state);

// Call this after owner has been assigned to persist settings into
// DeviceSettings storage.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SETTINGS)
void Finalize(PrefService* local_state);

// Used to convert |policy| into a string that is saved to prefs.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SETTINGS)
std::string PolicyDataToString(const enterprise_management::PolicyData& policy);

}  // namespace ash::device_settings_cache

#endif  // CHROMEOS_ASH_COMPONENTS_SETTINGS_DEVICE_SETTINGS_CACHE_H_
