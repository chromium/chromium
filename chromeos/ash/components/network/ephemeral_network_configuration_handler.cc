// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/ephemeral_network_configuration_handler.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_policy_observer.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"

namespace ash {

// static
std::unique_ptr<EphemeralNetworkConfigurationHandler>
EphemeralNetworkConfigurationHandler::TryCreate(
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    bool was_enterprise_managed_at_startup) {
  // LoginState may be missing in unit tests.
  if (!LoginState::IsInitialized()) {
    CHECK_IS_TEST();
    return {};
  }
  return base::WrapUnique(new EphemeralNetworkConfigurationHandler(
      managed_network_configuration_handler,
      was_enterprise_managed_at_startup));
}

EphemeralNetworkConfigurationHandler::EphemeralNetworkConfigurationHandler(
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    bool was_enterprise_managed_at_startup)
    : managed_network_configuration_handler_(
          managed_network_configuration_handler),
      was_enterprise_managed_at_startup_(was_enterprise_managed_at_startup) {
  DCHECK(policy_util::AreEphemeralNetworkPoliciesEnabled());

  CHECK(LoginState::IsInitialized());
  login_state_observation_.Observe(LoginState::Get());
  network_policy_observation_.Observe(managed_network_configuration_handler_);

  ReevaluateIsActive();
}

EphemeralNetworkConfigurationHandler::~EphemeralNetworkConfigurationHandler() =
    default;

void EphemeralNetworkConfigurationHandler::LoggedInStateChanged() {
  ReevaluateIsActive();
}

void EphemeralNetworkConfigurationHandler::TriggerPoliciesChangedForTesting(
    const std::string& userhash) {
  PoliciesChanged(userhash);
}

void EphemeralNetworkConfigurationHandler::PoliciesChanged(
    const std::string& userhash) {
  // The ephemeral network configuration logic only applies to the shared
  // profile.
  if (!userhash.empty()) {
    return;
  }

  ReevaluateIsActive();
}

void EphemeralNetworkConfigurationHandler::ReevaluateIsActive() {
  // The sign-in screen check is performed to avoid triggering wiping of
  // ephemeral network data in active sessions, e.g. when ash-chrome restarts
  // due to a crash.
  const bool is_on_login_screen = !LoginState::Get()->IsUserLoggedIn();
  if (!is_on_login_screen) {
    // Can stop observing login state now that the sign-in screen has exited.
    login_state_observation_.Reset();
  }

  const bool any_ephemeral_policy_enabled =
      managed_network_configuration_handler_->RecommendedValuesAreEphemeral() ||
      managed_network_configuration_handler_
          ->UserCreatedNetworkConfigurationsAreEphemeral();

  const bool should_be_active =
      is_on_login_screen && any_ephemeral_policy_enabled;

  if (!active_ && should_be_active) {
    Activate();
  } else if (active_ && !should_be_active) {
    Deactivate();
  }
}

void EphemeralNetworkConfigurationHandler::Activate() {
  DCHECK(!active_);
  active_ = true;

  chromeos::PowerManagerClient* const power_manager_client =
      chromeos::PowerManagerClient::Get();
  CHECK(power_manager_client);
  power_manager_client_observation_.Observe(power_manager_client);

  // Trigger initial wiping of ephemeral network data if the device was already
  // enterprise-enrolled and this is the first policy application where
  // ephemeral actions should be active.
  //
  // Wiping on enterprise enrollment is skipped to avoid a disruptive network
  // change when enterprise enrollment is performed on a user-created network
  // configuration.
  //
  // Note that if another one of the policies becomes active after this already
  // happened, its enforcement will not be re-triggered on the policy change but
  // only on the next regular trigger (ash-chrome start / wake up from sleep).
  // This could be changed in the future.
  if (was_enterprise_managed_at_startup_ && !has_applied_ephemeral_policies_) {
    has_applied_ephemeral_policies_ = true;
    managed_network_configuration_handler_
        ->TriggerEphemeralNetworkConfigActions();
  }
}

void EphemeralNetworkConfigurationHandler::Deactivate() {
  DCHECK(active_);
  active_ = false;

  power_manager_client_observation_.Reset();
}

void EphemeralNetworkConfigurationHandler::SuspendDone(
    base::TimeDelta sleep_duration) {
  // A zero value for the suspend duration indicates that the suspend was
  // canceled. Ignore the notification if that's the case.
  if (sleep_duration.is_zero()) {
    return;
  }
  if (!active_) {
    return;
  }
  has_applied_ephemeral_policies_ = true;
  managed_network_configuration_handler_
      ->TriggerEphemeralNetworkConfigActions();
}

void EphemeralNetworkConfigurationHandler::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& screen_idle_state) {
  if (!screen_idle_state.off()) {
    return;
  }
  if (!active_) {
    return;
  }
  has_applied_ephemeral_policies_ = true;
  managed_network_configuration_handler_
      ->TriggerEphemeralNetworkConfigActions();
}

}  // namespace ash
