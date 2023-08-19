// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_UTILS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_UTILS_H_

#include "components/policy/policy_export.h"

class PrefService;

namespace policy::utils {

// Returns a boolean representing whether enable-policy-test-page is enabled.
bool POLICY_EXPORT IsPolicyTestingEnabled(PrefService* pref_service);

}  // namespace policy::utils

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_UTILS_H_
