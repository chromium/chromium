// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_ISOLATED_MODE_PREFS_H_
#define COMPONENTS_ENTERPRISE_ISOLATED_MODE_PREFS_H_

#include "components/prefs/pref_registry_simple.h"

namespace enterprise_isolated_mode {

// Pref that maps to the "IsolatedModeSettings" policy.
// It is an int-enum preference.
extern const char kEnterpriseIsolatedModeSettings[];

// Registers Enterprise Isolated Mode profile prefs under the given registry.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_isolated_mode

#endif  // COMPONENTS_ENTERPRISE_ISOLATED_MODE_PREFS_H_
