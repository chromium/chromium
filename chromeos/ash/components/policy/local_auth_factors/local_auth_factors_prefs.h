// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_PREFS_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_PREFS_H_

#include "base/component_export.h"

class PrefRegistrySimple;

namespace policy::local_auth_factors {

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace policy::local_auth_factors

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_LOCAL_AUTH_FACTORS_LOCAL_AUTH_FACTORS_PREFS_H_
