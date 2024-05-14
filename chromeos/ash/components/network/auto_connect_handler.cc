// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/auto_connect_handler.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

constexpr char kESimPolicyDisconnectByPolicyHistogram[] =
    "Network.Cellular.ESim.DisconnectByPolicy.Result";

constexpr char kPSimPolicyDisconnectByPolicyHistogram[] =
    "Network.Cellular.PSim.DisconnectByPolicy.Result";

void RecordDisconnectByPolicyResult(const NetworkState* network, bool success) {
  if (network->type() == shill::kTypeCellular) {
    if (network->eid().empty()) {
      base::UmaHistogramBoolean(kPSimPolicyDisconnectByPolicyHistogram,
                                success);
      return;
    }
    base::UmaHistogramBoolean(kESimPolicyDisconnectByPolicyHistogram, success);
  }
}

void DisconnectErrorCallback(const NetworkState* network,
                             const std::string& error_name) {
  RecordDisconnectByPolicyResult(network, /*success=*/false);

  NET_LOG(ERROR) << "AutoConnectHandler.Disconnect failed for: "
                 << NetworkPathId(network->path())
                 << " Error name: " << error_name;
}

void RemoveNetworkConfigurationErrorCallback(const std::string& error_name) {
  NET_LOG(ERROR) << "AutoConnectHandler RemoveNetworkConfiguration failed."
                 << " Error name: " << error_name;
}

void ConnectToNetworkErrorCallback(const std::string& error_name) {
  NET_LOG(ERROR) << "AutoConnectHandler ConnectToNetwork failed."
                 << " Error name: " << error_name;
}

void SetPropertiesErrorCallback(const std::string& error_name) {
  NET_LOG(ERROR) << "AutoConnectHandler SetProperties failed."
                 << " Error name: " << error_name;
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
      applied_autoconnect_policy_on_wifi(false),
      applied_autoconnect_policy_on_cellular(false),
      auto_connect_reasons_(0) {}

AutoConnectHandler::~AutoConnectHandler() {
  if (LoginState::IsInitialized())
    LoginState::Get()->RemoveObserver(this);
  if (client_cert_resolver_)
    client_cert_resolver_->RemoveObserver(this);
  if (network_connection_handler_)
    network_connection_handler_->RemoveObserver(this);
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
  if (network_state_handler_) {
    network_state_handler_observer_.Observe(network_state_handler_.get());
  }

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
  DisconnectWiFiIfPolicyRequires();
  DisconnectCellularIfPolicyRequires();
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

  DisconnectWiFiIfPolicyRequires();
  DisconnectCellularIfPolicyRequires();

  // Request to connect to the best network only if there is at least one
  // managed network. Otherwise only process existing requests.
  if (managed_configuration_handler_->HasAnyPolicyNetwork(userhash)) {
    RequestBestConnection(
        AutoConnectReason::AUTO_CONNECT_REASON_POLICY_APPLIED);
  } else {
    CheckBestConnection();
  }
}

void AutoConnectHandler::ScanCompleted(const DeviceState* device) {
  if (device->type() != shill::kTypeWifi)
    return;

  // Enforce AllowOnlyPolicyWiFiToConnectIfAvailable policy if enabled.
  const NetworkState* managed_network =
      network_state_handler_->GetAvailableManagedWifiNetwork();
  if (device_policy_applied_ && user_policy_applied_ && managed_network &&
      managed_configuration_handler_
          ->AllowOnlyPolicyWiFiToConnectIfAvailable()) {
    const NetworkState* connected_network =
        network_state_handler_->ConnectedNetworkByType(
            NetworkTypePattern::WiFi());
    if (connected_network && !connected_network->IsManagedByPolicy()) {
      network_connection_handler_->ConnectToNetwork(
          managed_network->path(), base::DoNothing(),
          base::BindOnce(&ConnectToNetworkErrorCallback), false,
          ConnectCallbackMode::ON_COMPLETED);
      return;
    }
  }
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
  for (auto& observer : observer_list_) {
    observer.OnAutoConnectedInitiated(auto_connect_reasons);
  }

  if (network_connection_handler_) {
    network_connection_handler_->OnAutoConnectedInitiated(auto_connect_reasons);
  }
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

  // Request ScanAndConnectToBestServices after processing any pending DBus
  // calls.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&AutoConnectHandler::CallShillScanAndConnectToBestServices,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutoConnectHandler::DisconnectWiFiIfPolicyRequires() {
  // Wait for both (user & device) policies to be applied. The device policy
  // holds all the policies, which might require disconnects, while the user
  // policy might allow some networks again. This also ensures that we only
  // disconnect from blocked networks in user sessions.
  if (!device_policy_applied_ || !user_policy_applied_)
    return;

  std::vector<std::string> blocked_hex_ssids =
      managed_configuration_handler_->GetBlockedHexSSIDs();
  bool only_managed_wifi =
      managed_configuration_handler_->AllowOnlyPolicyWiFiToConnect();
  bool only_managed_autoconnect =
      managed_configuration_handler_->AllowOnlyPolicyNetworksToAutoconnect();
  bool available_only =
      managed_configuration_handler_
          ->AllowOnlyPolicyWiFiToConnectIfAvailable() &&
      network_state_handler_->GetAvailableManagedWifiNetwork();

  // Enforce the autoconnect-policy on WiFi networks only once.
  if (applied_autoconnect_policy_on_wifi)
    only_managed_autoconnect = false;
  else
    applied_autoconnect_policy_on_wifi = only_managed_autoconnect;

  // Early exit if no policy is set that requires any disconnects.
  if (!only_managed_wifi && !only_managed_autoconnect &&
      blocked_hex_ssids.empty() && !available_only) {
    return;
  }

  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::WiFi(),
      /*configure_only=*/false, /*visible=*/false, /*limit=*/0, &networks);

  DisconnectAndRemoveConfigOrDisableAutoConnect(
      networks, only_managed_autoconnect, available_only);
}

void AutoConnectHandler::DisconnectCellularIfPolicyRequires() {
  bool only_managed_cellular =
      managed_configuration_handler_->AllowOnlyPolicyCellularNetworks();
  bool only_managed_autoconnect =
      managed_configuration_handler_->AllowOnlyPolicyNetworksToAutoconnect();

  // Enforce the autoconnect-policy on cellular networks only once.
  if (applied_autoconnect_policy_on_cellular)
    only_managed_autoconnect = false;
  else
    applied_autoconnect_policy_on_cellular = only_managed_autoconnect;

  // Early exit if no policy is set that requires any disconnects.
  if (!only_managed_cellular && !only_managed_autoconnect) {
    return;
  }

  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Cellular(),
      /*configure_only=*/false, /*visible=*/false, /*limit=*/0, &networks);
  DisconnectAndRemoveConfigOrDisableAutoConnect(
      networks, only_managed_autoconnect, /*available_only=*/false);
}

void AutoConnectHandler::DisconnectAndRemoveConfigOrDisableAutoConnect(
    const NetworkStateHandler::NetworkStateList& networks,
    bool only_managed_autoconnect,
    bool available_only) {
  for (const NetworkState* network : networks) {
    if (network->IsManagedByPolicy())
      continue;

    bool is_cellular_type = network->type() == shill::kTypeCellular;
    if (network->blocked_by_policy()) {
      // Disconnect blocked network.
      if (network->IsConnectingOrConnected())
        DisconnectNetwork(network);

      // Disable auto connect property for all unmanaged cellular networks.
      // Otherwise if only removes its network configuration, turn off and on
      // cellular device will enforce modem connect to unmanaged cellular
      // network.
      if (is_cellular_type) {
        DisableAutoconnectForNetwork(network->path(),
                                     ::onc::network_config::kCellular);
      } else if (network->IsInProfile() && !available_only) {
        RemoveNetworkConfigurationForNetwork(network->path());
      }
    } else if (only_managed_autoconnect) {
      // Disconnect & disable auto-connect.
      if (network->IsConnectingOrConnected())
        DisconnectNetwork(network);
      if (network->IsInProfile())
        DisableAutoconnectForNetwork(
            network->path(), is_cellular_type ? ::onc::network_config::kCellular
                                              : ::onc::network_config::kWiFi);
    }
  }
}

void AutoConnectHandler::DisconnectNetwork(const NetworkState* network) {
  NET_LOG(EVENT) << "Disconnect forced by policy for: "
                 << NetworkPathId(network->path());
  network_connection_handler_->DisconnectNetwork(
      network->path(),
      base::BindOnce(&RecordDisconnectByPolicyResult, network,
                     /*success=*/true),
      base::BindOnce(&DisconnectErrorCallback, network));
}

void AutoConnectHandler::RemoveNetworkConfigurationForNetwork(
    const std::string& service_path) {
  NET_LOG(EVENT) << "Remove configuration forced by policy for: "
                 << NetworkPathId(service_path);
  managed_configuration_handler_->RemoveConfiguration(
      service_path, base::DoNothing(),
      base::BindOnce(&RemoveNetworkConfigurationErrorCallback));
}

void AutoConnectHandler::DisableAutoconnectForNetwork(
    const std::string& service_path,
    const std::string& network_type) {
  NET_LOG(EVENT) << "Disable auto-connect forced by policy: "
                 << NetworkPathId(service_path);
  base::Value::Dict properties;

  std::string autoconnect_path;
  if (network_type == ::onc::network_config::kWiFi) {
    autoconnect_path = base::StrCat(
        {::onc::network_config::kWiFi, ".", ::onc::wifi::kAutoConnect});
  } else if (network_type == ::onc::network_config::kCellular) {
    autoconnect_path = base::StrCat(
        {::onc::network_config::kCellular, ".", ::onc::cellular::kAutoConnect});
  } else {
    NOTREACHED_IN_MIGRATION();
  }
  properties.SetByDottedPath(autoconnect_path, false);
  managed_configuration_handler_->SetProperties(
      service_path, properties, base::DoNothing(),
      base::BindOnce(&SetPropertiesErrorCallback));
}

void AutoConnectHandler::CallShillScanAndConnectToBestServices() {
  NET_LOG(EVENT) << "ScanAndConnectToBestServices ["
                 << AutoConnectReasonsToString(auto_connect_reasons_) << "]";

  ShillManagerClient::Get()->ScanAndConnectToBestServices(
      base::BindOnce(&AutoConnectHandler::NotifyAutoConnectInitiated,
                     weak_ptr_factory_.GetWeakPtr(), auto_connect_reasons_),
      base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                     "ConnectToBestServices Failed", "",
                     network_handler::ErrorCallback()));
}

}  // namespace ash
