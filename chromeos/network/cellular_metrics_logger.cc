// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_metrics_logger.h"

#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chromeos/network/cellular_esim_profile.h"
#include "chromeos/network/cellular_esim_profile_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace {

// Checks whether the current logged in user type is an owner or regular.
bool IsLoggedInUserOwnerOrRegular() {
  if (!LoginState::IsInitialized())
    return false;

  LoginState::LoggedInUserType user_type =
      LoginState::Get()->GetLoggedInUserType();
  return user_type == LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER ||
         user_type == LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR;
}

}  // namespace

// static
const base::TimeDelta CellularMetricsLogger::kInitializationTimeout =
    base::TimeDelta::FromSeconds(15);

// static
const base::TimeDelta CellularMetricsLogger::kDisconnectRequestTimeout =
    base::TimeDelta::FromSeconds(5);

CellularMetricsLogger::ConnectionInfo::ConnectionInfo(
    const std::string& network_guid,
    bool is_connected)
    : network_guid(network_guid), is_connected(is_connected) {}

CellularMetricsLogger::ConnectionInfo::ConnectionInfo(
    const std::string& network_guid)
    : network_guid(network_guid) {}

CellularMetricsLogger::ConnectionInfo::~ConnectionInfo() = default;

CellularMetricsLogger::CellularMetricsLogger() = default;

CellularMetricsLogger::~CellularMetricsLogger() {
  if (network_state_handler_)
    OnShuttingDown();

  if (initialized_) {
    if (LoginState::IsInitialized())
      LoginState::Get()->RemoveObserver(this);

    if (network_connection_handler_)
      network_connection_handler_->RemoveObserver(this);
  }
}

void CellularMetricsLogger::Init(
    NetworkStateHandler* network_state_handler,
    NetworkConnectionHandler* network_connection_handler,
    CellularESimProfileHandler* cellular_esim_profile_handler) {
  network_state_handler_ = network_state_handler;
  cellular_esim_profile_handler_ = cellular_esim_profile_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);

  if (network_connection_handler) {
    network_connection_handler_ = network_connection_handler;
    network_connection_handler_->AddObserver(this);
  }

  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  // Devices and networks may already be present before this method is called.
  // Make sure that lists and timers are initialized properly.
  DeviceListChanged();
  NetworkListChanged();
  initialized_ = true;
}

void CellularMetricsLogger::DeviceListChanged() {
  NetworkStateHandler::DeviceStateList device_list;
  network_state_handler_->GetDeviceListByType(NetworkTypePattern::Cellular(),
                                              &device_list);
  bool new_is_cellular_available = !device_list.empty();
  if (is_cellular_available_ == new_is_cellular_available)
    return;

  is_cellular_available_ = new_is_cellular_available;
  // Start a timer to wait for cellular networks to initialize.
  // This makes sure that intermediate not-connected states are
  // not logged before initialization is completed.
  if (is_cellular_available_) {
    initialization_timer_.Start(
        FROM_HERE, kInitializationTimeout, this,
        &CellularMetricsLogger::OnInitializationTimeout);
  }
}

void CellularMetricsLogger::NetworkListChanged() {
  base::flat_map<std::string, std::unique_ptr<ConnectionInfo>>
      old_connection_info_map;
  // Clear |guid_to_connection_info_map| so that only new and existing
  // networks are added back to it.
  old_connection_info_map.swap(guid_to_connection_info_map_);

  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);

  // Check the current cellular networks list and copy existing connection info
  // from old map to new map or create new ones if it does not exist.
  for (const auto* network : network_list) {
    const std::string& guid = network->guid();
    auto old_connection_info_map_iter = old_connection_info_map.find(guid);
    if (old_connection_info_map_iter != old_connection_info_map.end()) {
      guid_to_connection_info_map_.insert_or_assign(
          guid, std::move(old_connection_info_map_iter->second));
      continue;
    }

    guid_to_connection_info_map_.insert_or_assign(
        guid,
        std::make_unique<ConnectionInfo>(guid, network->IsConnectedState()));
  }
}

void CellularMetricsLogger::OnInitializationTimeout() {
  CheckForPSimActivationStateMetric();
  CheckForESimProfileStatusMetric();
  CheckForCellularUsageCountMetric();
  CheckForCellularServiceCountMetric();
}

void CellularMetricsLogger::LoggedInStateChanged() {
  if (!IsLoggedInUserOwnerOrRegular())
    return;

  // This flag enures that activation state is only logged once when
  // the user logs in.
  is_psim_activation_state_logged_ = false;
  CheckForPSimActivationStateMetric();

  // This flag enures that activation state is only logged once when
  // the user logs in.
  is_esim_profile_status_logged_ = false;
  CheckForESimProfileStatusMetric();

  // This flag ensures that the service count is only logged once when
  // the user logs in.
  is_service_count_logged_ = false;
  CheckForCellularServiceCountMetric();
}

void CellularMetricsLogger::NetworkConnectionStateChanged(
    const NetworkState* network) {
  DCHECK(network_state_handler_);
  CheckForCellularUsageCountMetric();

  if (network->type().empty() ||
      !network->Matches(NetworkTypePattern::Cellular())) {
    return;
  }

  CheckForTimeToConnectedMetric(network);
  CheckForConnectionStateMetric(network);
}

void CellularMetricsLogger::CheckForTimeToConnectedMetric(
    const NetworkState* network) {
  if (network->activation_state() != shill::kActivationStateActivated)
    return;

  // We could be receiving a connection state change for a network different
  // from the one observed when the start time was recorded. Make sure that we
  // only look up time to connected of the corresponding network.
  ConnectionInfo* connection_info =
      GetConnectionInfoForCellularNetwork(network->guid());

  if (network->IsConnectingState()) {
    if (!connection_info->last_connect_start_time.has_value())
      connection_info->last_connect_start_time = base::TimeTicks::Now();

    return;
  }

  if (!connection_info->last_connect_start_time.has_value())
    return;

  if (network->IsConnectedState()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Network.Cellular.Connection.TimeToConnected",
        base::TimeTicks::Now() - *connection_info->last_connect_start_time);
  }

  // This is hit when the network is no longer in connecting state,
  // successfully connected or otherwise. Reset the connect start_time
  // so that it is not used for further connection state changes.
  connection_info->last_connect_start_time.reset();
}

void CellularMetricsLogger::DisconnectRequested(
    const std::string& service_path) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network->Matches(NetworkTypePattern::Cellular()))
    return;

  ConnectionInfo* connection_info =
      GetConnectionInfoForCellularNetwork(network->guid());

  // A disconnect request could fail and result in no cellular connection state
  // change. Save the request time so that only disconnections that do not
  // correspond to a request received within |kDisconnectRequestTimeout| are
  // tracked.
  connection_info->last_disconnect_request_time = base::TimeTicks::Now();
}

CellularMetricsLogger::PSimActivationState
CellularMetricsLogger::PSimActivationStateToEnum(const std::string& state) {
  if (state == shill::kActivationStateActivated)
    return PSimActivationState::kActivated;
  else if (state == shill::kActivationStateActivating)
    return PSimActivationState::kActivating;
  else if (state == shill::kActivationStateNotActivated)
    return PSimActivationState::kNotActivated;
  else if (state == shill::kActivationStatePartiallyActivated)
    return PSimActivationState::kPartiallyActivated;

  return PSimActivationState::kUnknown;
}

void CellularMetricsLogger::LogCellularDisconnectionsHistogram(
    ConnectionState connection_state) {
  UMA_HISTOGRAM_ENUMERATION("Network.Cellular.Connection.Disconnections",
                            connection_state);
}

void CellularMetricsLogger::CheckForConnectionStateMetric(
    const NetworkState* network) {
  ConnectionInfo* connection_info =
      GetConnectionInfoForCellularNetwork(network->guid());

  bool new_is_connected = network->IsConnectedState();
  if (connection_info->is_connected == new_is_connected)
    return;
  base::Optional<bool> old_is_connected = connection_info->is_connected;
  connection_info->is_connected = new_is_connected;

  if (new_is_connected) {
    LogCellularDisconnectionsHistogram(ConnectionState::kConnected);
    connection_info->last_disconnect_request_time.reset();
    return;
  }

  // If the previous connection state is nullopt then this is a new connection
  // info entry and a disconnection did not really occur. Skip logging the
  // metric in this case.
  if (!old_is_connected.has_value())
    return;

  base::Optional<base::TimeDelta> time_since_disconnect_requested;
  if (connection_info->last_disconnect_request_time) {
    time_since_disconnect_requested =
        base::TimeTicks::Now() - *connection_info->last_disconnect_request_time;
  }

  // If the disconnect occurred in less than |kDisconnectRequestTimeout|
  // from the last disconnect request time then treat it as a user
  // initiated disconnect and skip histogram log.
  if (time_since_disconnect_requested &&
      time_since_disconnect_requested < kDisconnectRequestTimeout) {
    return;
  }
  LogCellularDisconnectionsHistogram(ConnectionState::kDisconnected);
}

void CellularMetricsLogger::CheckForESimProfileStatusMetric() {
  if (!cellular_esim_profile_handler_ || !is_cellular_available_ ||
      is_esim_profile_status_logged_ || !IsLoggedInUserOwnerOrRegular()) {
    return;
  }

  std::vector<CellularESimProfile> esim_profiles =
      cellular_esim_profile_handler_->GetESimProfiles();

  bool pending_profiles_exist = false;
  bool active_profiles_exist = false;
  for (const auto& profile : esim_profiles) {
    switch (profile.state()) {
      case CellularESimProfile::State::kPending:
        FALLTHROUGH;
      case CellularESimProfile::State::kInstalling:
        pending_profiles_exist = true;
        break;

      case CellularESimProfile::State::kInactive:
        FALLTHROUGH;
      case CellularESimProfile::State::kActive:
        active_profiles_exist = true;
        break;
    }
  }

  ESimProfileStatus activation_state;
  if (active_profiles_exist && !pending_profiles_exist)
    activation_state = ESimProfileStatus::kActive;
  else if (active_profiles_exist && pending_profiles_exist)
    activation_state = ESimProfileStatus::kActiveWithPendingProfiles;
  else if (!active_profiles_exist && pending_profiles_exist)
    activation_state = ESimProfileStatus::kPendingProfilesOnly;
  else
    activation_state = ESimProfileStatus::kNoProfiles;

  UMA_HISTOGRAM_ENUMERATION("Network.Cellular.ESim.StatusAtLogin",
                            activation_state);
  is_esim_profile_status_logged_ = true;
}

void CellularMetricsLogger::CheckForPSimActivationStateMetric() {
  if (!is_cellular_available_ || is_psim_activation_state_logged_ ||
      !IsLoggedInUserOwnerOrRegular())
    return;

  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);

  if (network_list.size() == 0)
    return;

  base::Optional<std::string> psim_activation_state;
  for (const auto* network : network_list) {
    if (network->eid().empty())
      psim_activation_state = network->activation_state();
  }

  // No PSim networks exist.
  if (!psim_activation_state.has_value())
    return;

  UMA_HISTOGRAM_ENUMERATION("Network.Cellular.PSim.StatusAtLogin",
                            PSimActivationStateToEnum(*psim_activation_state));
  is_psim_activation_state_logged_ = true;
}

void CellularMetricsLogger::CheckForCellularServiceCountMetric() {
  if (!is_cellular_available_ || is_service_count_logged_ ||
      !IsLoggedInUserOwnerOrRegular()) {
    return;
  }

  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);

  size_t psim_networks = 0;
  size_t esim_profiles = 0;

  for (const auto* network : network_list) {
    if (!network->eid().empty())
      esim_profiles++;
    else
      psim_networks++;
  }

  UMA_HISTOGRAM_COUNTS_100("Network.Cellular.PSim.ServiceAtLogin.Count",
                           psim_networks);
  UMA_HISTOGRAM_COUNTS_100("Network.Cellular.ESim.ServiceAtLogin.Count",
                           esim_profiles);
  is_service_count_logged_ = true;
}

void CellularMetricsLogger::CheckForCellularUsageCountMetric() {
  if (!is_cellular_available_)
    return;

  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::NonVirtual(), &network_list);

  base::Optional<const NetworkState*> connected_cellular_network;
  bool is_non_cellular_connected = false;
  for (auto* network : network_list) {
    if (!network->IsConnectedState())
      continue;

    // Note: Only one cellular network may be ever connected.
    if (network->Matches(NetworkTypePattern::Cellular()))
      connected_cellular_network = network;
    else
      is_non_cellular_connected = true;
  }

  // Discard not-connected states received before the timer runs out.
  if (!connected_cellular_network.has_value() &&
      initialization_timer_.IsRunning()) {
    return;
  }

  CellularUsage usage;
  base::Optional<SimType> sim_type;
  if (connected_cellular_network.has_value()) {
    usage = is_non_cellular_connected
                ? CellularUsage::kConnectedWithOtherNetwork
                : CellularUsage::kConnectedAndOnlyNetwork;
    sim_type = connected_cellular_network.value()->eid().empty()
                   ? SimType::kPSim
                   : SimType::kESim;
  } else {
    usage = CellularUsage::kNotConnected;
  }

  if (!sim_type.has_value() || *sim_type == SimType::kPSim) {
    if (usage != last_psim_cellular_usage_)
      UMA_HISTOGRAM_ENUMERATION("Network.Cellular.PSim.Usage.Count", usage);
    last_psim_cellular_usage_ = usage;
  }

  if (!sim_type.has_value() || *sim_type == SimType::kESim) {
    if (usage != last_esim_cellular_usage_)
      UMA_HISTOGRAM_ENUMERATION("Network.Cellular.ESim.Usage.Count", usage);
    last_esim_cellular_usage_ = usage;
  }
}

CellularMetricsLogger::ConnectionInfo*
CellularMetricsLogger::GetConnectionInfoForCellularNetwork(
    const std::string& cellular_network_guid) {
  auto it = guid_to_connection_info_map_.find(cellular_network_guid);

  ConnectionInfo* connection_info;
  if (it == guid_to_connection_info_map_.end()) {
    // We could get connection events in some cases before network
    // list change event. Insert new network into the list.
    auto insert_result = guid_to_connection_info_map_.insert_or_assign(
        cellular_network_guid,
        std::make_unique<ConnectionInfo>(cellular_network_guid));
    connection_info = insert_result.first->second.get();
  } else {
    connection_info = it->second.get();
  }

  return connection_info;
}

void CellularMetricsLogger::OnShuttingDown() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  network_state_handler_ = nullptr;
}

}  // namespace chromeos
