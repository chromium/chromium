// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_COMMON_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_COMMON_H_

#include "components/policy/policy_export.h"

namespace policy {

class PolicyMap;

// Blocks sensitive policies from having an effect in the specified source.
// Modifies the |policy| in place. Only call this if the source is untrusted.
POLICY_EXPORT void FilterSensitivePolicies(PolicyMap* policy);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_COMMON_H_
