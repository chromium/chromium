// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/wifi_hotspot_connector.h"

#include <memory>

#include "base/bind.h"
#include "base/guid.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/shill_property_util.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace tether {

WifiHotspotConnector::WifiHotspotConnector(
    NetworkStateHandler* network_state_handler,
    NetworkConnect* network_connect)
    : network_state_handler_(network_state_handler),
      network_connect_(network_connect),
      timer_(std::make_unique<base::OneShotTimer>()),
      clock_(base::DefaultClock::GetInstance()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  network_state_handler_->AddObserver(this, FROM_HERE);
}

WifiHotspotConnector::~WifiHotspotConnector() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);

  // If a connection attempt is active when this class is destroyed, the attempt
  // has no time to finish successfully, so it is considered a failure.
  if (!wifi_network_guid_.empty())
    CompleteActiveConnectionAttempt(false /* success */);
}

void WifiHotspotConnector::ConnectToWifiHotspot(
    const std::string& ssid,
    const std::string& password,
    const std::string& tether_network_guid,
    const WifiConnectionCallback& callback) {
  DCHECK(!ssid.empty());
  // Note: |password| can be empty in some cases.

  if (!callback_.is_null()) {
    DCHECK(timer_->IsRunning());

    // If another connection attempt was underway but had not yet completed,
    // disassociate that network from the Tether network and call the callback,
    // passing an empty string to signal that the connection did not complete
    // successfully.
    bool successful_disassociation =
        network_state_handler_->DisassociateTetherNetworkStateFromWifiNetwork(
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

    CompleteActiveConnectionAttempt(false /* success */);
  }

  ssid_ = ssid;
  password_ = password;
  tether_network_guid_ = tether_network_guid;
  wifi_network_guid_ = base::GenerateGUID();
  callback_ = callback;
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromSeconds(kConnectionTimeoutSeconds),
                base::Bind(&WifiHotspotConnector::OnConnectionTimeout,
                           weak_ptr_factory_.GetWeakPtr()));
  connection_attempt_start_time_ = clock_->Now();

  // If Wi-Fi is enabled, continue with creating the configuration of the
  // hotspot. Otherwise, request that Wi-Fi be enabled and wait; see
  // UpdateWaitingForWifi.
  if (network_state_handler_->IsTechnologyEnabled(NetworkTypePattern::WiFi())) {
    // Ensure that a possible previous pending callback to UpdateWaitingForWifi
    // won't result in a second call to CreateWifiConfiguration().
    is_waiting_for_wifi_to_enable_ = false;

    CreateWifiConfiguration();
  } else if (!is_waiting_for_wifi_to_enable_) {
    is_waiting_for_wifi_to_enable_ = true;

    // Once Wi-Fi is enabled, UpdateWaitingForWifi will be called.
    network_state_handler_->SetTechnologyEnabled(
        NetworkTypePattern::WiFi(), true /*enabled */,
        base::Bind(&WifiHotspotConnector::OnEnableWifiError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void WifiHotspotConnector::OnEnableWifiError(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  is_waiting_for_wifi_to_enable_ = false;
  PA_LOG(ERROR) << "Failed to enable Wi-Fi: " << error_name;
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

  if (network->IsConnectedState()) {
    // The network has connected, so complete the connection attempt. Because
    // this is a NetworkStateHandlerObserver callback, complete the attempt in
    // a new task to ensure that NetworkStateHandler is not modified while it is
    // notifying observers. See https://crbug.com/800370.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WifiHotspotConnector::CompleteActiveConnectionAttempt,
                       weak_ptr_factory_.GetWeakPtr(), true /* success */));
    return;
  }

  if (network->connectable() && !has_initiated_connection_to_current_network_) {
    // The network is connectable, so initiate a connection attempt. Because
    // this is a NetworkStateHandlerObserver callback, complete the attempt in
    // a new task to ensure that NetworkStateHandler is not modified while it is
    // notifying observers. See https://crbug.com/800370.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WifiHotspotConnector::InitiateConnectionToCurrentNetwork,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void WifiHotspotConnector::DevicePropertiesUpdated(const DeviceState* device) {
  if (device->Matches(NetworkTypePattern::WiFi()))
    UpdateWaitingForWifi();
}

void WifiHotspotConnector::UpdateWaitingForWifi() {
  if (!is_waiting_for_wifi_to_enable_ ||
      !network_state_handler_->IsTechnologyEnabled(
          NetworkTypePattern::WiFi())) {
    return;
  }

  is_waiting_for_wifi_to_enable_ = false;

  if (ssid_.empty())
    return;

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WifiHotspotConnector::CreateWifiConfiguration,
                                weak_ptr_factory_.GetWeakPtr()));
}

void WifiHotspotConnector::InitiateConnectionToCurrentNetwork() {
  if (wifi_network_guid_.empty()) {
    PA_LOG(WARNING) << "InitiateConnectionToCurrentNetwork() was called, but "
                    << "the connection was canceled before it could be "
                    << "initiated.";
    return;
  }

  // Set |has_initiated_connection_to_current_network_| to true to ensure that
  // this code path is only run once per connection attempt. Without this
  // field, the association and connection code below would be re-run multiple
  // times for one network.
  has_initiated_connection_to_current_network_ = true;

  // If the network is now connectable, associate it with a Tether network
  // ASAP so that the correct icon will be displayed in the tray while the
  // network is connecting.
  bool successful_association =
      network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
          tether_network_guid_, wifi_network_guid_);
  if (successful_association) {
    PA_LOG(VERBOSE) << "Wi-Fi network (ID \"" << wifi_network_guid_ << "\") "
                    << "successfully associated with Tether network (ID \""
                    << tether_network_guid_ << "\"). Starting connection "
                    << "attempt.";
  } else {
    PA_LOG(ERROR) << "Wi-Fi network (ID \"" << wifi_network_guid_ << "\") "
                  << "failed to associate with Tether network (ID \""
                  << tether_network_guid_ << "\"). Starting connection "
                  << "attempt.";
  }

  // Initiate a connection to the network.
  network_connect_->ConnectToNetworkId(wifi_network_guid_);
}

void WifiHotspotConnector::CompleteActiveConnectionAttempt(bool success) {
  if (wifi_network_guid_.empty()) {
    PA_LOG(WARNING) << "CompleteActiveConnectionAttempt(" << success << ") "
                    << "was called, but no connection attempt is in progress.";
    return;
  }

  // Note: Empty string is passed to callback to signify a failed attempt.
  std::string wifi_guid_for_callback =
      success ? wifi_network_guid_ : std::string();

  // If the connection attempt has failed (e.g., due to cancellation or
  // timeout) and ConnectToNetworkId() has already been called, the in-progress
  // connection attempt should be stopped; there is no cancellation mechanism,
  // so DisconnectFromNetworkId() is called here instead. Without this, it would
  // be possible for the connection to complete after the Tether component had
  // already shut down. See crbug.com/761569.
  if (!success && has_initiated_connection_to_current_network_)
    network_connect_->DisconnectFromNetworkId(wifi_network_guid_);

  ssid_.clear();
  password_.clear();
  wifi_network_guid_.clear();
  has_initiated_connection_to_current_network_ = false;
  is_waiting_for_wifi_to_enable_ = false;

  timer_->Stop();

  if (!wifi_guid_for_callback.empty()) {
    // UMA_HISTOGRAM_MEDIUM_TIMES is used because UMA_HISTOGRAM_TIMES has a max
    // of 10 seconds.
    DCHECK(!connection_attempt_start_time_.is_null());
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "InstantTethering.Performance.ConnectToHotspotDuration",
        clock_->Now() - connection_attempt_start_time_);
    connection_attempt_start_time_ = base::Time();
  }

  callback_.Run(wifi_guid_for_callback);
  callback_.Reset();
}

void WifiHotspotConnector::CreateWifiConfiguration() {
  base::DictionaryValue properties =
      CreateWifiPropertyDictionary(ssid_, password_);

  // This newly configured network will eventually be passed as an argument to
  // NetworkPropertiesUpdated().
  network_connect_->CreateConfiguration(&properties, false /* shared */);
}

base::DictionaryValue WifiHotspotConnector::CreateWifiPropertyDictionary(
    const std::string& ssid,
    const std::string& password) {
  PA_LOG(VERBOSE) << "Creating network configuration. "
                  << "SSID: " << ssid << ", "
                  << "Password: " << password << ", "
                  << "Wi-Fi network GUID: " << wifi_network_guid_;

  base::DictionaryValue properties;

  shill_property_util::SetSSID(ssid, &properties);
  properties.SetKey(shill::kGuidProperty, base::Value(wifi_network_guid_));
  properties.SetKey(shill::kAutoConnectProperty, base::Value(false));
  properties.SetKey(shill::kTypeProperty, base::Value(shill::kTypeWifi));
  properties.SetKey(shill::kSaveCredentialsProperty, base::Value(true));

  if (password.empty()) {
    properties.SetKey(shill::kSecurityClassProperty,
                      base::Value(shill::kSecurityNone));
  } else {
    properties.SetKey(shill::kSecurityClassProperty,
                      base::Value(shill::kSecurityPsk));
    properties.SetKey(shill::kPassphraseProperty, base::Value(password));
  }

  return properties;
}

void WifiHotspotConnector::OnConnectionTimeout() {
  CompleteActiveConnectionAttempt(false /* success */);
}

void WifiHotspotConnector::SetTestDoubles(
    std::unique_ptr<base::OneShotTimer> test_timer,
    base::Clock* test_clock,
    scoped_refptr<base::TaskRunner> test_task_runner) {
  timer_ = std::move(test_timer);
  clock_ = test_clock;
  task_runner_ = test_task_runner;
}

}  // namespace tether

}  // namespace chromeos
