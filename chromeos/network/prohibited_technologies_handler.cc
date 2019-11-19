// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/prohibited_technologies_handler.h"

#include <set>
#include <vector>

#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

ProhibitedTechnologiesHandler::ProhibitedTechnologiesHandler() = default;

ProhibitedTechnologiesHandler::~ProhibitedTechnologiesHandler() {
  if (managed_network_configuration_handler_)
    managed_network_configuration_handler_->RemoveObserver(this);
  if (LoginState::IsInitialized())
    LoginState::Get()->RemoveObserver(this);
}

void ProhibitedTechnologiesHandler::Init(
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    NetworkStateHandler* network_state_handler) {
  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  managed_network_configuration_handler_ =
      managed_network_configuration_handler;
  if (managed_network_configuration_handler_)
    managed_network_configuration_handler_->AddObserver(this);
  network_state_handler_ = network_state_handler;

  // Clear the list of prohibited network technologies. As a user logout always
  // triggers a browser process restart, Init() is always invoked to reallow any
  // network technology forbidden for the previous user.
  network_state_handler_->SetProhibitedTechnologies(
      std::vector<std::string>(), chromeos::network_handler::ErrorCallback());

  if (LoginState::IsInitialized())
    LoggedInStateChanged();
}

void ProhibitedTechnologiesHandler::LoggedInStateChanged() {
  user_logged_in_ = LoginState::Get()->IsUserLoggedIn();
  EnforceProhibitedTechnologies();
}

void ProhibitedTechnologiesHandler::PoliciesChanged(
    const std::string& userhash) {}

void ProhibitedTechnologiesHandler::PoliciesApplied(
    const std::string& userhash) {
  if (userhash.empty())
    return;
  user_policy_applied_ = true;
  EnforceProhibitedTechnologies();
}

void ProhibitedTechnologiesHandler::SetProhibitedTechnologies(
    const base::ListValue* prohibited_list) {
  // Build up prohibited network type list and save it for furthur use when
  // enforced
  session_prohibited_technologies_.clear();
  for (const auto& item : *prohibited_list) {
    std::string prohibited_technology;
    bool item_is_string = item.GetAsString(&prohibited_technology);
    DCHECK(item_is_string);
    std::string translated_tech =
        network_util::TranslateONCTypeToShill(prohibited_technology);
    if (!translated_tech.empty())
      session_prohibited_technologies_.push_back(translated_tech);
  }
  EnforceProhibitedTechnologies();
}

void ProhibitedTechnologiesHandler::EnforceProhibitedTechnologies() {
  auto prohibited_technologies_ = GetCurrentlyProhibitedTechnologies();
  network_state_handler_->SetProhibitedTechnologies(
      prohibited_technologies_, network_handler::ErrorCallback());
  // Enable ethernet back as user doesn't have a place to enable it back
  // if user shuts down directly in a user session. As shill will persist
  // ProhibitedTechnologies which may include ethernet, making users can
  // not find Ethernet at next boot or logging out unless user log out first
  // and then shutdown.
  if (std::find(prohibited_technologies_.begin(),
                prohibited_technologies_.end(),
                shill::kTypeEthernet) != prohibited_technologies_.end()) {
    return;
  }
  if (network_state_handler_->IsTechnologyAvailable(
          NetworkTypePattern::Ethernet()) &&
      !network_state_handler_->IsTechnologyEnabled(
          NetworkTypePattern::Ethernet())) {
    network_state_handler_->SetTechnologyEnabled(
        NetworkTypePattern::Ethernet(), true, network_handler::ErrorCallback());
  }
}

std::vector<std::string>
ProhibitedTechnologiesHandler::GetCurrentlyProhibitedTechnologies() {
  if (!user_logged_in_ || !user_policy_applied_)
    return globally_prohibited_technologies_;
  std::set<std::string> prohibited_set;
  prohibited_set.insert(session_prohibited_technologies_.begin(),
                        session_prohibited_technologies_.end());
  prohibited_set.insert(globally_prohibited_technologies_.begin(),
                        globally_prohibited_technologies_.end());
  std::vector<std::string> prohibited_list(prohibited_set.begin(),
                                           prohibited_set.end());
  return prohibited_list;
}

void ProhibitedTechnologiesHandler::AddGloballyProhibitedTechnology(
    const std::string& technology) {
  if (std::find(globally_prohibited_technologies_.begin(),
                globally_prohibited_technologies_.end(),
                technology) == globally_prohibited_technologies_.end()) {
    globally_prohibited_technologies_.push_back(technology);
  }
  EnforceProhibitedTechnologies();
}

void ProhibitedTechnologiesHandler::RemoveGloballyProhibitedTechnology(
    const std::string& technology) {
  auto it = std::find(globally_prohibited_technologies_.begin(),
                      globally_prohibited_technologies_.end(), technology);
  if (it != globally_prohibited_technologies_.end())
    globally_prohibited_technologies_.erase(it);
  EnforceProhibitedTechnologies();
}

}  // namespace chromeos
