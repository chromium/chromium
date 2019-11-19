// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_PROHIBITED_TECHNOLOGIES_HANDLER_H_
#define CHROMEOS_NETWORK_PROHIBITED_TECHNOLOGIES_HANDLER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_policy_observer.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) ProhibitedTechnologiesHandler
    : public LoginState::Observer,
      public NetworkPolicyObserver {
 public:
  ~ProhibitedTechnologiesHandler() override;

  // LoginState::Observer
  void LoggedInStateChanged() override;

  // NetworkPolicyObserver
  void PoliciesChanged(const std::string& userhash) override;
  void PoliciesApplied(const std::string& userhash) override;
  // Function for updating the list of technologies that are prohibited during
  // user sessions
  void SetProhibitedTechnologies(const base::ListValue* prohibited_list);
  // Functions for updating the list of technologies that are prohibited
  // everywhere, including login screen
  void AddGloballyProhibitedTechnology(const std::string& prohibited_list);
  void RemoveGloballyProhibitedTechnology(const std::string& technology);
  // Returns the currently active list of prohibited
  // technologies(session-dependent and globally-prohibited ones)
  std::vector<std::string> GetCurrentlyProhibitedTechnologies();

 private:
  friend class NetworkHandler;
  friend class ProhibitedTechnologiesHandlerTest;

  ProhibitedTechnologiesHandler();

  void Init(
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      NetworkStateHandler* network_state_handler);

  void EnforceProhibitedTechnologies();

  // These only apply in user or kiosk sessions.
  std::vector<std::string> session_prohibited_technologies_;
  // Network technologies that are prohibited not only in user sessions, but at
  // all time.
  std::vector<std::string> globally_prohibited_technologies_;

  ManagedNetworkConfigurationHandler* managed_network_configuration_handler_ =
      nullptr;
  NetworkStateHandler* network_state_handler_ = nullptr;
  bool user_logged_in_ = false;
  bool user_policy_applied_ = false;

  DISALLOW_COPY_AND_ASSIGN(ProhibitedTechnologiesHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_PROHIBITED_TECHNOLOGIES_HANDLER_H_
