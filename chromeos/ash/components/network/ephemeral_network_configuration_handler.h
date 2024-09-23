// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_EPHEMERAL_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_EPHEMERAL_NETWORK_CONFIGURATION_HANDLER_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_policy_observer.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace power_manager {
class ScreenIdleState;
}  // namespace power_manager

namespace ash {

// Triggers ephemeral network configuration actions on the sign-in screen when
// one of the ephemeral network policies is set to true and a one of the
// following triggers happen:
// - Initial policy application on startup IFF the device had already been
//   enterprise enrolled,
// - Device comes back from sleep,
// - Screen is turned off due to inactivity.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) EphemeralNetworkConfigurationHandler
    : public LoginState::Observer,
      public NetworkPolicyObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  // Attempts to create an EphemeralNetworkConfigurationHandler.
  // Can return nullptr in tests if dependencies are not initialized.
  // `managed_network_configuration_handler` must outlive the lifetime of this
  // object.
  // `was_enterprise_managed_at_startup` should be true if the device was
  // already enterprise-enrolled at ash-chrome startup time.
  static std::unique_ptr<EphemeralNetworkConfigurationHandler> TryCreate(
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      bool was_enterprise_managed_at_startup);

  ~EphemeralNetworkConfigurationHandler() override;

  EphemeralNetworkConfigurationHandler(
      const EphemeralNetworkConfigurationHandler&) = delete;
  EphemeralNetworkConfigurationHandler& operator=(
      const EphemeralNetworkConfigurationHandler&) = delete;

  void TriggerPoliciesChangedForTesting(const std::string& userhash);

 private:
  EphemeralNetworkConfigurationHandler(
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      bool was_enterprise_managed_at_startup);

  // Re-evaluates if this EphemeralNetworkConfigurationHandler should be active
  // and call Active/Deactivate accordingly.
  void ReevaluateIsActive();

  // Activate this EphemeralNetworkConfigurationHandler (start watching for
  // triggers to trigger ephemeral network config actions) and also perform the
  // initial trigger if not yet done.
  void Activate();

  // Deactivate this EphemeralNetworkConfigurationHandler - it will not be
  // triggering ephemeral network config actions while deactivated.
  void Deactivate();

  // LoginState::Observer:
  void LoggedInStateChanged() override;

  // NetworkPolicyObserver:
  void PoliciesChanged(const std::string& userhash) override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta sleep_duration) override;
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& screen_idle_state) override;

  // Unowned.
  raw_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;

  base::ScopedObservation<LoginState, LoginState::Observer>
      login_state_observation_{this};
  base::ScopedObservation<ManagedNetworkConfigurationHandler,
                          NetworkPolicyObserver>
      network_policy_observation_{this};
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};

  const bool was_enterprise_managed_at_startup_;

  // This is true when any ephemeral network settings are active - i.e. the
  // device is not in a user session and one of the ephemeral network
  // configuration ONC policies is enabled.
  bool active_ = false;

  // This is true if ephemeral policy application has been triggered at least
  // once by this class in `managed_network_configuration_handler_`.
  bool has_applied_ephemeral_policies_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_EPHEMERAL_NETWORK_CONFIGURATION_HANDLER_H_
