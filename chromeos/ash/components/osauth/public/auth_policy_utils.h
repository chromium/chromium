// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_POLICY_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_POLICY_UTILS_H_

#include <optional>

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/prefs/pref_service.h"

namespace cryptohome {
class AuthFactor;
}

namespace ash {

// Takes in a list of a policy values and maps the result into an auth factors
// set.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
std::optional<AuthFactorsSet> GetAuthFactorsSetFromPolicyList(
    const base::ListValue* policy_allowed_auth_factors);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
bool IsPinEnabledAsMainFactorByPolicy(const PrefService* pref_service);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
bool HasPinFactor(const base::ListValue* auth_factors);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
bool IsGaiaPassword(const cryptohome::AuthFactor& factor);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
bool IsLocalPassword(const cryptohome::AuthFactor& factor);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_POLICY_UTILS_H_
