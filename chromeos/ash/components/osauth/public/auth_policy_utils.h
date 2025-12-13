// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_POLICY_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_POLICY_UTILS_H_

#include <optional>

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// Takes in a list of a policy values and maps the result into an auth factors
// set.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
std::optional<AuthFactorsSet> GetAuthFactorsSetFromPolicyList(
    const base::Value::List* policy_allowed_auth_factors);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_POLICY_UTILS_H_
