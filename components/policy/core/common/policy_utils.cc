// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_utils.h"

#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"

namespace policy::utils {

bool IsPolicyTestingEnabled(PrefService* pref_service) {
  bool flag_enabled =
      base::FeatureList::IsEnabled(policy::features::kEnablePolicyTestPage);
  bool policy_enabled =
      pref_service->GetBoolean(policy_prefs::kPolicyTestPageEnabled);
  return flag_enabled && policy_enabled;
}

}  // namespace policy::utils
