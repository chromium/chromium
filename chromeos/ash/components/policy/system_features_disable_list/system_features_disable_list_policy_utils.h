// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_SYSTEM_FEATURES_DISABLE_LIST_SYSTEM_FEATURES_DISABLE_LIST_POLICY_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_SYSTEM_FEATURES_DISABLE_LIST_SYSTEM_FEATURES_DISABLE_LIST_POLICY_UTILS_H_

#include "base/component_export.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

// Registers prefs corresponding to the SystemFeaturesDisableList and
// SystemFeaturesDisableMode policies.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
void RegisterDisabledSystemFeaturesPrefs(PrefRegistrySimple* registry);

// Whether the icons of apps disabled by the SystemFeaturesDisableList policy
// are hidden or blocked.
// In managed guest sessions (MGS), this is configured by policy
// (blocked by default). In regular user sessions, icons are hidden by default.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
bool IsDisabledAppsModeHidden(const PrefService& local_state);

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_SYSTEM_FEATURES_DISABLE_LIST_SYSTEM_FEATURES_DISABLE_LIST_POLICY_UTILS_H_
