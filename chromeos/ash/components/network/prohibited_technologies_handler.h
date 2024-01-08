// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_PROHIBITED_TECHNOLOGIES_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_PROHIBITED_TECHNOLOGIES_HANDLER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_policy_observer.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) ProhibitedTechnologiesHandler
    : public LoginState::Observer,
      public NetworkPolicyObserver {
 public:
  ProhibitedTechnologiesHandler(const ProhibitedTechnologiesHandler&) = delete;
  ProhibitedTechnologiesHandler& operator=(
      const ProhibitedTechnologiesHandler&) = delete;

  ~ProhibitedTechnologiesHandler() override;

  // LoginState::Observer
  void LoggedInStateChanged() override;

  // NetworkPolicyObserver
  void PoliciesChanged(const std::string& userhash) override;
  void PoliciesApplied(const std::string& userhash) override;
  // Function for updating the list of technologies that are prohibited during
  // user sessions
  void SetProhibitedTechnologies(const base::Value::List& prohibited_list);
  // Functions for updating the list of technologies that are prohibited
  // everywhere, including login screen
  void AddGloballyProhibitedTechnology(const std::string& technology);
  void RemoveGloballyProhibitedTechnology(const std::string& technology);
  // Returns the currently active list of prohibited
  // technologies(session-dependent and globally-prohibited ones)
  std::vector<std::string> GetCurrentlyProhibitedTechnologies();

 private:
  friend class ManagedNetworkConfigurationHandlerTest;
  friend class NetworkHandler;
  friend class ProhibitedTechnologiesHandlerTest;

  ProhibitedTechnologiesHandler();

  void Init(
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      NetworkStateHandler* network_state_handler,
      TechnologyStateController* technology_state_controller);

  void EnforceProhibitedTechnologies();

  // These only apply in user or kiosk sessions.
  std::vector<std::string> session_prohibited_technologies_;
  // Network technologies that are prohibited not only in user sessions, but at
  // all time.
  std::vector<std::string> globally_prohibited_technologies_;

  raw_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_ = nullptr;
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  raw_ptr<TechnologyStateController> technology_state_controller_ = nullptr;
  bool user_logged_in_ = false;
  bool user_policy_applied_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_PROHIBITED_TECHNOLOGIES_HANDLER_H_
