// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/auto_connect_handler.h"

#include <sstream>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_type_pattern.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void DisconnectErrorCallback(
    const std::string& network_path,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  std::stringstream error_data_ss;
  if (error_data)
    error_data_ss << *error_data;
  else
    error_data_ss << "<none>";

  NET_LOG(ERROR) << "AutoConnectHandler.Disconnect failed. "
                 << "Path: \"" << network_path << "\", "
                 << "Error name: \"" << error_name << "\", "
                 << "Error data: " << error_data_ss.str();
}

void RemoveNetworkConfigurationErrorCallback(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  std::stringstream error_data_ss;
  if (error_data)
    error_data_ss << *error_data;
  else
    error_data_ss << "<none>";
  NET_LOG(ERROR) << "AutoConnectHandler RemoveNetworkConfiguration failed. "
                 << "Error name: \"" << error_name << "\", "
                 << "Error data: " << error_data_ss.str();
}

void ConnectToNetworkErrorCallback(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  std::stringstream error_data_ss;
  if (error_data)
    error_data_ss << *error_data;
  else
    error_data_ss << "<none>";
  NET_LOG(ERROR) << "AutoConnectHandler ConnectToNetwork failed. "
                 << "Error name: \"" << error_name << "\", "
                 << "Error data: " << error_data_ss.str();
}

void SetPropertiesErrorCallback(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  std::stringstream error_data_ss;
  if (error_data)
    error_data_ss << *error_data;
  else
    error_data_ss << "<none>";
  NET_LOG(ERROR) << "AutoConnectHandler SetProperties failed. "
                 << "Error name: \"" << error_name << "\", "
                 << "Error data: " << error_data_ss.str();
}

std::string AutoConnectReasonsToString(int auto_connect_reasons) {
  std::string result;

  if (auto_connect_reasons &
      AutoConnectHandler::AutoConnectReason::AUTO_CONNECT_REASON_LOGGED_IN) {
    result += "Logged In";
  }

  if (auto_connect_reasons & AutoConnectHandler::AutoConnectReason::
                                 AUTO_CONNECT_REASON_POLICY_APPLIED) {
    if (!result.empty())
      result += ", ";
    result += "Policy Applied";
  }

  if (auto_connect_reasons & AutoConnectHandler::AutoConnectReason::
                                 AUTO_CONNECT_REASON_CERTIFICATE_RESOLVED) {
    if (!result.empty())
      result += ", ";
    result += "Certificate resolved";
  }

  return result;
}

}  // namespace

AutoConnectHandler::AutoConnectHandler()
    : client_cert_resolver_(nullptr),
      request_best_connection_pending_(false),
      device_policy_applied_(false),
      user_policy_applied_(false),
      client_certs_resolved_(false),
      applied_autoconnect_policy_(false),
      connect_to_best_services_after_scan_(false),
      auto_connect_reasons_(0) {}

AutoConnectHandler::~AutoConnectHandler() {
  if (LoginState::IsInitialized())
    LoginState::Get()->RemoveObserver(this);
  if (client_cert_resolver_)
    client_cert_resolver_->RemoveObserver(this);
  if (network_connection_handler_)
    network_connection_handler_->RemoveObserver(this);
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (managed_configuration_handler_)
    managed_configuration_handler_->RemoveObserver(this);
}

void AutoConnectHandler::Init(
    ClientCertResolver* client_cert_resolver,
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  client_cert_resolver_ = client_cert_resolver;
  if (client_cert_resolver_)
    client_cert_resolver_->AddObserver(this);

  network_connection_handler_ = network_connection_handler;
  if (network_connection_handler_)
    network_connection_handler_->AddObserver(this);

  network_state_handler_ = network_state_handler;
  if (network_state_handler_)
    network_state_handler_->AddObserver(this, FROM_HERE);

  managed_configuration_handler_ = managed_network_configuration_handler;
  if (managed_configuration_handler_)
    managed_configuration_handler_->AddObserver(this);

  if (LoginState::IsInitialized())
    LoggedInStateChanged();
}

void AutoConnectHandler::LoggedInStateChanged() {
  if (!LoginState::Get()->IsUserLoggedIn())
    return;

  // Disconnect before connecting, to ensure that we do not disconnect a network
  // that we just connected.
  DisconnectIfPolicyRequires();
  RequestBestConnection(AutoConnectReason::AUTO_CONNECT_REASON_LOGGED_IN);
}

void AutoConnectHandler::ConnectToNetworkRequested(
    const std::string& /*service_path*/) {
  // Stop any pending request to connect to the best newtork.
  request_best_connection_pending_ = false;
}

void AutoConnectHandler::PoliciesApplied(const std::string& userhash) {
  if (userhash.empty())
    device_policy_applied_ = true;
  else
    user_policy_applied_ = true;

  DisconnectIfPolicyRequires();

  // Request to connect to the best network only if there is at least one
  // managed network. Otherwise only process existing requests.
  const ManagedNetworkConfigurationHandler::GuidToPolicyMap* managed_networks =
      managed_configuration_handler_->GetNetworkConfigsFromPolicy(userhash);
  DCHECK(managed_networks);
  if (!managed_networks->empty()) {
    RequestBestConnection(
        AutoConnectReason::AUTO_CONNECT_REASON_POLICY_APPLIED);
  } else {
    CheckBestConnection();
  }
}

void AutoConnectHandler::ScanCompleted(const DeviceState* device) {
  if (device->type() != shill::kTypeWifi)
    return;

  // Enforce AllowOnlyPolicyNetworksToConnectIfAvailable policy if enabled.
  const NetworkState* managed_network =
      network_state_handler_->GetAvailableManagedWifiNetwork();
  if (device_policy_applied_ && user_policy_applied_ && managed_network &&
      managed_configuration_handler_
          ->AllowOnlyPolicyNetworksToConnectIfAvailable()) {
    const NetworkState* connected_network =
        network_state_handler_->ConnectedNetworkByType(
            NetworkTypePattern::WiFi());
    if (connected_network && !connected_network->IsManagedByPolicy()) {
      network_connection_handler_->ConnectToNetwork(
          managed_network->path(), base::DoNothing(),
          base::Bind(&ConnectToNetworkErrorCallback), false,
          ConnectCallbackMode::ON_COMPLETED);
      return;
    }
  }

  if (!connect_to_best_services_after_scan_)
    return;

  connect_to_best_services_after_scan_ = false;
  // Request ConnectToBestServices after processing any pending DBus calls.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&AutoConnectHandler::CallShillConnectToBestServices,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutoConnectHandler::ResolveRequestCompleted(
    bool network_properties_changed) {
  client_certs_resolved_ = true;

  // Only request to connect to the best network if network properties were
  // actually changed. Otherwise only process existing requests.
  if (network_properties_changed) {
    RequestBestConnection(
        AutoConnectReason::AUTO_CONNECT_REASON_CERTIFICATE_RESOLVED);
  } else {
    CheckBestConnection();
  }
}

void AutoConnectHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AutoConnectHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AutoConnectHandler::NotifyAutoConnectInitiatedForTest(
    int auto_connect_reasons) {
  NotifyAutoConnectInitiated(auto_connect_reasons);
}

void AutoConnectHandler::NotifyAutoConnectInitiated(int auto_connect_reasons) {
  NET_LOG(EVENT) << "AutoConnectInitiated ["
                 << AutoConnectReasonsToString(auto_connect_reasons_) << "]";
  for (auto& observer : observer_list_)
    observer.OnAutoConnectedInitiated(auto_connect_reasons);
}

void AutoConnectHandler::RequestBestConnection(
    AutoConnectReason auto_connect_reason) {
  request_best_connection_pending_ = true;
  auto_connect_reasons_ |= auto_connect_reason;
  CheckBestConnection();
}

void AutoConnectHandler::CheckBestConnection() {
  // Return immediately if there is currently no request pending to change to
  // the best network.
  if (!request_best_connection_pending_)
    return;

  bool policy_application_running =
      managed_configuration_handler_->IsAnyPolicyApplicationRunning();
  bool client_cert_resolve_task_running =
      client_cert_resolver_->IsAnyResolveTaskRunning();
  VLOG(2) << "device policy applied: " << device_policy_applied_
          << "\nuser policy applied: " << user_policy_applied_
          << "\npolicy application running: " << policy_application_running
          << "\nclient cert patterns resolved: " << client_certs_resolved_
          << "\nclient cert resolve task running: "
          << client_cert_resolve_task_running;
  if (!device_policy_applied_ || policy_application_running ||
      client_cert_resolve_task_running) {
    return;
  }

  if (LoginState::Get()->IsUserLoggedIn()) {
    // Before changing connection after login, we wait at least for:
    //  - user policy applied at least once
    //  - client certificate patterns resolved
    if (!user_policy_applied_ || !client_certs_resolved_)
      return;
  }

  request_best_connection_pending_ = false;

  // Trigger a ConnectToBestNetwork request after the next scan completion.
  // Note: there is an edge case here if a scan is in progress and a hidden
  // network has been configured since the scan started. crbug.com/433075.
  if (connect_to_best_services_after_scan_)
    return;
  connect_to_best_services_after_scan_ = true;
  if (!network_state_handler_->GetScanningByType(
          NetworkTypePattern::Primitive(shill::kTypeWifi))) {
    network_state_handler_->RequestScan(NetworkTypePattern::WiFi());
  }
}

void AutoConnectHandler::DisconnectIfPolicyRequires() {
  // Wait for both (user & device) policies to be applied. The device policy
  // holds all the policies, which might require disconnects, while the user
  // policy might whitelist some networks again. This also ensures that we only
  // disconnect from blocked networks in user sessions.
  if (!device_policy_applied_ || !user_policy_applied_)
    return;

  std::vector<std::string> blacklisted_hex_ssids =
      managed_configuration_handler_->GetBlacklistedHexSSIDs();
  bool only_managed =
      managed_configuration_handler_->AllowOnlyPolicyNetworksToConnect();
  bool only_managed_autoconnect =
      managed_configuration_handler_->AllowOnlyPolicyNetworksToAutoconnect();
  bool available_only =
      managed_configuration_handler_
          ->AllowOnlyPolicyNetworksToConnectIfAvailable() &&
      network_state_handler_->GetAvailableManagedWifiNetwork();

  // Enforce the autoconnect-policy only once.
  if (applied_autoconnect_policy_)
    only_managed_autoconnect = false;
  else
    applied_autoconnect_policy_ = only_managed_autoconnect;

  // Early exit if no policy is set that requires any disconnects.
  if (!only_managed && !only_managed_autoconnect &&
      blacklisted_hex_ssids.empty() && !available_only) {
    return;
  }

  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(NetworkTypePattern::WiFi(),
                                               false, false, 0, &networks);
  for (const NetworkState* network : networks) {
    if (network->IsManagedByPolicy())
      continue;

    if (network->blocked_by_policy()) {
      // Disconnect & remove configuration.
      if (network->IsConnectingOrConnected())
        DisconnectNetwork(network->path());
      if (network->IsInProfile() && !available_only)
        RemoveNetworkConfigurationForNetwork(network->path());
    } else if (only_managed_autoconnect) {
      // Disconnect & disable auto-connect.
      if (network->IsConnectingOrConnected())
        DisconnectNetwork(network->path());
      if (network->IsInProfile())
        DisableAutoconnectForWiFiNetwork(network->path());
    }
  }
}

void AutoConnectHandler::DisconnectNetwork(const std::string& service_path) {
  NET_LOG_EVENT("Disconnect forced by policy", service_path);

  network_connection_handler_->DisconnectNetwork(
      service_path, base::DoNothing(),
      base::Bind(&DisconnectErrorCallback, service_path));
}

void AutoConnectHandler::RemoveNetworkConfigurationForNetwork(
    const std::string& service_path) {
  NET_LOG_EVENT("Remove configuration forced by policy", service_path);

  managed_configuration_handler_->RemoveConfiguration(
      service_path, base::DoNothing(),
      base::Bind(&RemoveNetworkConfigurationErrorCallback));
}

void AutoConnectHandler::DisableAutoconnectForWiFiNetwork(
    const std::string& service_path) {
  NET_LOG_EVENT("Disable auto-connect forced by policy", service_path);

  base::DictionaryValue properties;
  properties.SetPath({::onc::network_config::kWiFi, ::onc::wifi::kAutoConnect},
                     base::Value(false));
  managed_configuration_handler_->SetProperties(
      service_path, properties, base::DoNothing(),
      base::Bind(&SetPropertiesErrorCallback));
}

void AutoConnectHandler::CallShillConnectToBestServices() {
  NET_LOG(EVENT) << "ConnectToBestServices ["
                 << AutoConnectReasonsToString(auto_connect_reasons_) << "]";

  ShillManagerClient::Get()->ConnectToBestServices(
      base::Bind(&AutoConnectHandler::NotifyAutoConnectInitiated,
                 weak_ptr_factory_.GetWeakPtr(), auto_connect_reasons_),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "ConnectToBestServices Failed", "",
                 network_handler::ErrorCallback()));
}

}  // namespace chromeos
