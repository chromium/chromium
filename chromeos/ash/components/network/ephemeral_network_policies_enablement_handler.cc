// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/ephemeral_network_policies_enablement_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "components/prefs/pref_member.h"

namespace ash {

EphemeralNetworkPoliciesEnablementHandler::
    EphemeralNetworkPoliciesEnablementHandler(
        base::OnceClosure on_ephemeral_network_policies_enabled)
    : on_ephemeral_network_policies_enabled_(
          std::move(on_ephemeral_network_policies_enabled)) {
  DCHECK(on_ephemeral_network_policies_enabled_);
  if (features::AreEphemeralNetworkPoliciesEnabled()) {
    // Ephemeral network policies are enabled unconditionally.
    EnableEphemeralNetworkPolicies();
  }
}

EphemeralNetworkPoliciesEnablementHandler::
    ~EphemeralNetworkPoliciesEnablementHandler() = default;

void EphemeralNetworkPoliciesEnablementHandler::SetDevicePrefs(
    PrefService* device_prefs) {
  if (!device_prefs) {
    ephemeral_network_policies_enabled_pref_.reset();
    return;
  }

  if (policy_util::AreEphemeralNetworkPoliciesEnabled()) {
    // Already enabled - don't care about the policy value.
    return;
  }

  if (!features::CanEphemeralNetworkPoliciesBeEnabledByPolicy()) {
    // The policy should not be respected.
    return;
  }

  ephemeral_network_policies_enabled_pref_ =
      std::make_unique<BooleanPrefMember>();
  ephemeral_network_policies_enabled_pref_->Init(
      prefs::kDeviceEphemeralNetworkPoliciesEnabled, device_prefs,
      base::BindRepeating(
          &EphemeralNetworkPoliciesEnablementHandler::EvaluatePolicyValue,
          base::Unretained(this)));

  // Also evaluate the initial state of enablement.
  EvaluatePolicyValue();
}

void EphemeralNetworkPoliciesEnablementHandler::EvaluatePolicyValue() {
  // Only observing if the policy should be respected.
  DCHECK(features::CanEphemeralNetworkPoliciesBeEnabledByPolicy());

  // Don't do anything if ephemeral network policies are already enabled - can
  // only be disabled again by restarting ash-chrome.
  if (policy_util::AreEphemeralNetworkPoliciesEnabled()) {
    return;
  }

  if (ephemeral_network_policies_enabled_pref_ &&
      ephemeral_network_policies_enabled_pref_->GetValue()) {
    EnableEphemeralNetworkPolicies();
  }
}

void EphemeralNetworkPoliciesEnablementHandler::
    EnableEphemeralNetworkPolicies() {
  // Ephemeral network policies became enabled - mark as such and notify the
  // observer. This can only happen once per process lifetime (see the guard
  // clauses above).
  policy_util::SetEphemeralNetworkPoliciesEnabled();

  DCHECK(on_ephemeral_network_policies_enabled_);
  std::move(on_ephemeral_network_policies_enabled_).Run();

  // There's no need to watch the policy enablement anymore after ephemeral
  // network policies have been enabled.
  ephemeral_network_policies_enabled_pref_.reset();
}

}  // namespace ash
