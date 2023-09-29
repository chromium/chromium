// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_EPHEMERAL_NETWORK_POLICIES_ENABLEMENT_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_EPHEMERAL_NETWORK_POLICIES_ENABLEMENT_HANDLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "components/prefs/pref_member.h"

class PrefService;

namespace ash {

// EphemeralNetworkPoliciesEnablementHandler watches and propagates enablement
// of the "ephemeral network policies" feature.
// The feature can be enabled in two ways:
// - ash::feature::kEphemeralNetworkPolicies is enabled through the
//   experimentation framework (useful to enable the policies for the
//   whole population)
// or
// - ash::feature::kEphemeralNetworkPoliciesEnabledPolicy is enabled (default)
//   and the device policy DeviceEphemeralNetworkPoliciesEnabled is set to true
//   (so individual customers can enable the policies)
//
// When any of these two is detected, EphemeralNetworkPoliciesEnablementHandler
// sets the enablement in ash::policy_util::SetEphemeralNetworkPoliciesEnabled
// and notifies its observer.
//
// ash::policy_util is used to propagate the value for easy access from all
// ash network handling layers.
class COMPONENT_EXPORT(CHROMEOS_NETWORK)
    EphemeralNetworkPoliciesEnablementHandler {
 public:
  // `on_ephemeral_network_policies_enabled` will be called when ephemeral
  // network policies become enabled. This will be called at most once. It will
  // not be called after EphemeralNetworkPoliciesEnablementHandler has been
  // destroyed.
  EphemeralNetworkPoliciesEnablementHandler(
      base::OnceClosure on_ephemeral_network_policies_enabled);
  ~EphemeralNetworkPoliciesEnablementHandler();

  EphemeralNetworkPoliciesEnablementHandler(
      const EphemeralNetworkPoliciesEnablementHandler&) = delete;
  EphemeralNetworkPoliciesEnablementHandler& operator=(
      const EphemeralNetworkPoliciesEnablementHandler&) = delete;

  // `device_prefs` can be nullptr, e.g. when shutting down.
  void SetDevicePrefs(PrefService* device_prefs);

 private:
  // Called when a new DeviceEphemeralNetworkPoliciesEnabled policy value is
  // available.
  void EvaluatePolicyValue();

  // Enables ephemeral network policies and notifies the observer.
  // May only be called once.
  void EnableEphemeralNetworkPolicies();

  // To be called when ephemeral network policy support has been enabled.
  base::OnceClosure on_ephemeral_network_policies_enabled_;

  // Watches the pref mirroring the DeviceEphemeralNetworkPoliciesEnabled
  // policy.
  std::unique_ptr<BooleanPrefMember> ephemeral_network_policies_enabled_pref_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_EPHEMERAL_NETWORK_POLICIES_ENABLEMENT_HANDLER_H_
