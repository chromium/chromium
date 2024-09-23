// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/wifi_hotspot_connector.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::tether {

WifiHotspotConnector::WifiHotspotConnector(NetworkHandler* network_handler,
                                           NetworkConnect* network_connect)
    : network_connect_(network_connect),
      network_handler_(network_handler),
      timer_(std::make_unique<base::OneShotTimer>()),
      clock_(base::DefaultClock::GetInstance()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  network_state_handler_observer_.Observe(
      network_handler->network_state_handler());
}

WifiHotspotConnector::~WifiHotspotConnector() {
  // If a connection attempt is active when this class is destroyed, the attempt
  // has no time to finish successfully, so it is considered a failure.
  if (!wifi_network_guid_.empty()) {
    CompleteActiveConnectionAttempt(
        WifiHotspotConnectionError::kWifiHotspotConnectorClassDestroyed);
  }
}

void WifiHotspotConnector::ConnectToWifiHotspot(
    const std::string& ssid,
    const std::string& password,
    const std::string& tether_network_guid,
    WifiConnectionCallback callback) {
  DCHECK(!ssid.empty());
  // Note: |password| can be empty in some cases.

  if (callback_) {
    DCHECK(timer_->IsRunning());

    // If another connection attempt was underway but had not yet completed,
    // disassociate that network from the Tether network and call the callback,
    // passing an empty string to signal that the connection did not complete
    // successfully.
    bool successful_disassociation =
        network_handler_->network_state_handler()
            ->DisassociateTetherNetworkStateFromWifiNetwork(
                tether_network_guid_);
    if (successful_disassociation) {
      PA_LOG(VERBOSE) << "Wi-Fi network (ID \"" << wifi_network_guid_ << "\") "
                      << "successfully disassociated from Tether network (ID "
                      << "\"" << tether_network_guid_ << "\").";
    } else {
      PA_LOG(ERROR) << "Wi-Fi network (ID \"" << wifi_network_guid_ << "\") "
                    << "failed to disassociate from Tether network ID (\""
                    << tether_network_guid_ << "\").";
    }

    CompleteActiveConnectionAttempt(
        WifiHotspotConnectionError::kCancelledForNewerConnectionAttempt);
  }

  ssid_ = ssid;
  password_ = password;
  tether_network_guid_ = tether_network_guid;
  wifi_network_guid_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
  callback_ = std::move(callback);
  timer_->Start(FROM_HERE, base::Seconds(kConnectionTimeoutSeconds),
                base::BindOnce(&WifiHotspotConnector::OnConnectionTimeout,
                               weak_ptr_factory_.GetWeakPtr()));
  connection_attempt_start_time_ = clock_->Now();

  // If Wi-Fi is enabled, continue with creating the configuration of the
  // hotspot. Otherwise, request that Wi-Fi be enabled and wait; see
  // UpdateWaitingForWifi.
  if (network_handler_->network_state_handler()->IsTechnologyEnabled(
          NetworkTypePattern::WiFi())) {
    // Ensure that a possible previous pending callback to UpdateWaitingForWifi
    // won't result in a second call to CreateWifiConfiguration().
    is_waiting_for_wifi_to_enable_ = false;

    CreateWifiConfiguration();
  } else if (!is_waiting_for_wifi_to_enable_) {
    is_waiting_for_wifi_to_enable_ = true;

    // Once Wi-Fi is enabled, UpdateWaitingForWifi will be called.
    network_handler_->technology_state_controller()->SetTechnologiesEnabled(
        NetworkTypePattern::WiFi(), true /*enabled */,
        base::BindRepeating(&WifiHotspotConnector::OnEnableWifiError,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void WifiHotspotConnector::RequestWifiScan() {
  network_handler_->network_state_handler()->RequestScan(
      NetworkTypePattern::WiFi());
}

void WifiHotspotConnector::OnEnableWifiError(const std::string& error_name) {
  is_waiting_for_wifi_to_enable_ = false;
  PA_LOG(ERROR) << "Failed to enable Wi-Fi: " << error_name;
  CompleteActiveConnectionAttempt(
      WifiHotspotConnectionError::kWifiFailedToEnabled);
}

void WifiHotspotConnector::DeviceListChanged() {
  UpdateWaitingForWifi();
}

void WifiHotspotConnector::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (network->guid() != wifi_network_guid_) {
    // If a different network has been connected, return early and wait for the
    // network with ID |wifi_network_guid_| is updated.
    return;
  }

  if (!has_requested_wifi_scan_) {
    has_requested_wifi_scan_ = true;
    RequestWifiScan();
  }

  // We use "visible" to determine if the network can be connected to, rather
  // than "connectable". In the aftermath of b/302621170, it was discovered
  // "connectable" only refers to the service having a SSID and password set.
  // As the service is configured earlier, that check will always pass, even
  // if the service wasn't discovered by a scan. "Visible", by contrast, is
  // used by the UI to determine if the service should be shown. If it's shown,
  // we know it has been discovered by a fresh scan.
  if (network->visible() && !has_initiated_connection_to_current_network_) {
    // Set |has_initiated_connection_to_current_network_| to true to ensure that
    // this code path is only run once per connection attempt.
    has_initiated_connection_to_current_network_ = true;

    InitiateConnectionToCurrentNetwork();
  }
}

void WifiHotspotConnector::DevicePropertiesUpdated(const DeviceState* device) {
  if (device->Matches(NetworkTypePattern::WiFi())) {
    UpdateWaitingForWifi();
  }
}

void WifiHotspotConnector::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void WifiHotspotConnector::UpdateWaitingForWifi() {
  if (!is_waiting_for_wifi_to_enable_ ||
      !network_handler_->network_state_handler()->IsTechnologyEnabled(
          NetworkTypePattern::WiFi())) {
    return;
  }

  is_waiting_for_wifi_to_enable_ = false;

  if (ssid_.empty()) {
    return;
  }

  CreateWifiConfiguration();
}

void WifiHotspotConnector::InitiateConnectionToCurrentNetwork() {
  if (wifi_network_guid_.empty()) {
    PA_LOG(WARNING) << "InitiateConnectionToCurrentNetwork() was called, but "
                    << "the connection was canceled before it could be "
                    << "initiated.";
    return;
  }

  // If the network is now connectable, associate it with a Tether network
  // ASAP so that the correct icon will be displayed in the tray while the
  // network is connecting.
  // NOTE: AssociateTetherNetworkStateWithWifiNetwork() is idempotent, so
  // calling it on each retry is safe.
  // Because this method may be called by `NetworkPropertiesUpdated` (a method
  // on `NetworkStateHandlerObserver`), associate the network in a new task to
  // ensure that NetworkStateHandler is not modified while it is notifying
  // observers. See https://crbug.com/800370.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WifiHotspotConnector::AssociateNetworks,
                                weak_ptr_factory_.GetWeakPtr(),
                                wifi_network_guid_, tether_network_guid_));

  // Initiate a connection to the network.
  const NetworkState* network_state =
      network_handler_->network_state_handler()->GetNetworkStateFromGuid(
          wifi_network_guid_);

  if (!network_state) {
    PA_LOG(ERROR) << "Network state for " << wifi_network_guid_
                  << " was null. Failing.";
    CompleteActiveConnectionAttempt(
        WifiHotspotConnectionError::kNetworkStateWasNull);
  }

  PA_LOG(INFO) << "Current connection attempt is #"
               << current_connection_attempt_count_
               << ". Attempting to connect...";

  ++current_connection_attempt_count_;

  network_handler_->network_connection_handler()->ConnectToNetwork(
      network_state->path(),
      base::BindOnce(&WifiHotspotConnector::OnWifiConnectionSucceeded,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WifiHotspotConnector::OnWifiConnectionFailed,
                     weak_ptr_factory_.GetWeakPtr()),
      /*check_error_state=*/false, ConnectCallbackMode::ON_COMPLETED);
}

// TODO(b/318534727): Record the number of attempts before completion in a
// metric.
void WifiHotspotConnector::CompleteActiveConnectionAttempt(
    std::optional<WifiHotspotConnectionError> error) {
  if (wifi_network_guid_.empty()) {
    PA_LOG(ERROR) << "CompleteActiveConnectionAttempt"
                  << "was called, but no connection attempt is in progress.";
    if (error.has_value()) {
      PA_LOG(ERROR) << "CompleteActiveConnectionAttempt error: "
                    << error.value();
    } else {
      PA_LOG(ERROR) << "CompleteActiveConnectionAttempt had no error";
    }

    return;
  }

  PA_LOG(VERBOSE) << "Completing active connection attempt to network with ID: "
                  << wifi_network_guid_;

  // If the connection attempt has failed (e.g., due to cancellation or
  // timeout) and ConnectToNetworkId() has already been called, the in-progress
  // connection attempt should be stopped; there is no cancellation mechanism,
  // so DisconnectNetwork() is called here instead. Without this, it would
  // be possible for the connection to complete after the Tether component had
  // already shut down. See crbug.com/761569.
  if (error.has_value() && has_initiated_connection_to_current_network_) {
    const NetworkState* network_state =
        network_handler_->network_state_handler()->GetNetworkStateFromGuid(
            wifi_network_guid_);
    if (!network_state) {
      PA_LOG(ERROR) << "Network state for " << wifi_network_guid_
                    << " was null. Unable to disconnect.";
    } else {
      // TODO(b/313488946): Handle errors during disconnection on failure.
      network_handler_->network_connection_handler()->DisconnectNetwork(
          network_state->path(), /*success_callback=*/base::DoNothing(),
          /*error_callback=*/base::DoNothingAs<void(const std::string&)>());
    }
  }

  std::string wifi_network_guid_copy_for_callback_ = wifi_network_guid_;

  ssid_.clear();
  password_.clear();
  wifi_network_guid_.clear();
  has_initiated_connection_to_current_network_ = false;
  is_waiting_for_wifi_to_enable_ = false;
  has_requested_wifi_scan_ = false;
  current_connection_attempt_count_ = 0;

  timer_->Stop();

  if (error.has_value()) {
    std::move(callback_).Run(base::unexpected(error.value()));
    return;
  }

  // UMA_HISTOGRAM_MEDIUM_TIMES is used because UMA_HISTOGRAM_TIMES has a max
  // of 10 seconds.
  DCHECK(!connection_attempt_start_time_.is_null());
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "InstantTethering.Performance.ConnectToHotspotDuration",
      clock_->Now() - connection_attempt_start_time_);
  connection_attempt_start_time_ = base::Time();

  std::move(callback_).Run(base::ok(wifi_network_guid_copy_for_callback_));
}

void WifiHotspotConnector::CreateWifiConfiguration() {
  base::Value::Dict properties = CreateWifiPropertyDictionary(ssid_, password_);

  // This newly configured network will eventually be passed as an argument to
  // NetworkPropertiesUpdated().
  network_connect_->CreateConfiguration(std::move(properties),
                                        /* shared */ false);
}

base::Value::Dict WifiHotspotConnector::CreateWifiPropertyDictionary(
    const std::string& ssid,
    const std::string& password) {
  PA_LOG(VERBOSE) << "Creating network configuration. " << "SSID: " << ssid
                  << ", " << "Password: " << password << ", "
                  << "Wi-Fi network GUID: " << wifi_network_guid_;

  base::Value::Dict properties;

  shill_property_util::SetSSID(ssid, &properties);
  properties.Set(shill::kGuidProperty, wifi_network_guid_);
  properties.Set(shill::kAutoConnectProperty, false);
  properties.Set(shill::kTypeProperty, shill::kTypeWifi);
  properties.Set(shill::kSaveCredentialsProperty, true);

  if (password.empty()) {
    properties.Set(shill::kSecurityClassProperty, shill::kSecurityClassNone);
  } else {
    properties.Set(shill::kSecurityClassProperty, shill::kSecurityClassPsk);
    properties.Set(shill::kPassphraseProperty, password);
  }

  return properties;
}

void WifiHotspotConnector::OnConnectionTimeout() {
  CompleteActiveConnectionAttempt(WifiHotspotConnectionError::kTimeout);
}

void WifiHotspotConnector::OnWifiConnectionSucceeded() {
  // The network has connected, so complete the connection attempt. Because
  // this is a NetworkStateHandlerObserver callback, complete the attempt in
  // a new task to ensure that NetworkStateHandler is not modified while it is
  // notifying observers. See https://crbug.com/800370.
  PA_LOG(INFO) << "Successfully connected to Wifi Network";
  CompleteActiveConnectionAttempt(/*error=*/std::nullopt);
}

void WifiHotspotConnector::OnWifiConnectionFailed(
    const std::string& error_name) {
  PA_LOG(ERROR) << "Failed to connect to Wifi Network. Error: [" << error_name
                << "]";
  if (current_connection_attempt_count_ <= kMaxWifiConnectionAttempts) {
    PA_LOG(INFO) << "Current connection attempt is #"
                 << current_connection_attempt_count_ << ". Maximum is "
                 << kMaxWifiConnectionAttempts << ". Retrying Connection...";
    InitiateConnectionToCurrentNetwork();
    return;
  }

  // The network connect has failed, so complete the connection attempt. Because
  // this is a NetworkStateHandlerObserver callback, complete the attempt in
  // a new task to ensure that NetworkStateHandler is not modified while it is
  // notifying observers. See https://crbug.com/800370.
  PA_LOG(ERROR) << "Hit maximum allowed connection attempts. Failing.";
  CompleteActiveConnectionAttempt(
      WifiHotspotConnectionError::kNetworkConnectionHandlerFailed);
}

void WifiHotspotConnector::AssociateNetworks(std::string wifi_network_guid,
                                             std::string tether_network_guid) {
  if (!wifi_network_guid_.empty() && wifi_network_guid != wifi_network_guid_) {
    PA_LOG(INFO) << "Skipping association of [" << tether_network_guid
                 << "] with [" << wifi_network_guid
                 << "], as a newer connection attempt was scheduled before an "
                    "association could be made.";
    return;
  }

  bool successful_association =
      network_handler_->network_state_handler()
          ->AssociateTetherNetworkStateWithWifiNetwork(tether_network_guid,
                                                       wifi_network_guid);
  if (successful_association) {
    PA_LOG(VERBOSE) << "Wi-Fi network (ID \"" << wifi_network_guid << "\") "
                    << "successfully associated with Tether network (ID \""
                    << tether_network_guid << "\"). Starting connection "
                    << "attempt.";
  } else {
    PA_LOG(ERROR) << "Wi-Fi network (ID \"" << wifi_network_guid << "\") "
                  << "failed to associate with Tether network (ID \""
                  << tether_network_guid << "\"). Starting connection "
                  << "attempt.";
  }
}

void WifiHotspotConnector::SetTestDoubles(
    std::unique_ptr<base::OneShotTimer> test_timer,
    base::Clock* test_clock,
    scoped_refptr<base::TaskRunner> test_task_runner) {
  timer_ = std::move(test_timer);
  clock_ = test_clock;
  task_runner_ = test_task_runner;
}

std::ostream& operator<<(
    std::ostream& stream,
    const WifiHotspotConnector::WifiHotspotConnectionError error) {
  switch (error) {
    case WifiHotspotConnector::WifiHotspotConnectionError::kTimeout:
      stream << "[timeout]";
      break;
    case WifiHotspotConnector::WifiHotspotConnectionError::
        kCancelledForNewerConnectionAttempt:
      stream << "[cancelled for newer connection attempt]";
      break;
    case WifiHotspotConnector::WifiHotspotConnectionError::
        kWifiHotspotConnectorClassDestroyed:
      stream << "[WifiHotspotConnector destroyed]";
      break;
    case WifiHotspotConnector::WifiHotspotConnectionError::kNetworkStateWasNull:
      stream << "[network state was null]";
      break;
    case WifiHotspotConnector::WifiHotspotConnectionError::
        kNetworkConnectionHandlerFailed:
      stream << "[network connection handler failed to connect]";
      break;
    case WifiHotspotConnector::WifiHotspotConnectionError::kWifiFailedToEnabled:
      stream << "[wifi failed to enabled]";
      break;
  }

  return stream;
}

}  // namespace ash::tether
