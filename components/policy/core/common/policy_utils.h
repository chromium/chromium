// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_UTILS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_UTILS_H_

#include "components/policy/policy_export.h"
#include "components/version_info/channel.h"

class PrefService;

namespace policy::utils {

// Returns a boolean representing whether chrome://policy/test is available on
// `channel` and if it is not blocked by policy or a disabled feature flag. This
// page is available by default on Canary and exceptionally on Beta on iOS since
// iOS does not have a Canary.
bool POLICY_EXPORT IsPolicyTestingEnabled(PrefService* pref_service,
                                          version_info::Channel channel);

}  // namespace policy::utils

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_UTILS_H_
