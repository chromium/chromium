// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_state_handler.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/fake_network_state_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// Constants used for logging.
constexpr char kReasonStateChange[] = "State Change";
constexpr char kReasonChange[] = "New Network";
constexpr char kReasonUpdate[] = "Update";
constexpr char kReasonUpdateIPConfig[] = "UpdateIPConfig";
constexpr char kReasonUpdateDeviceIPConfig[] = "UpdateDeviceIPConfig";
constexpr char kReasonTether[] = "Tether Change";

bool ConnectionStateChanged(const NetworkState* network,
                            const std::string& prev_connection_state) {
  std::string connection_state = network->connection_state();
  bool prev_idle = prev_connection_state.empty() ||
                   prev_connection_state == shill::kStateIdle;
  bool cur_idle = connection_state == shill::kStateIdle;
  if (prev_idle || cur_idle) {
    return prev_idle != cur_idle;
  }
  return connection_state != prev_connection_state;
}

std::string GetManagedStateLogType(const ManagedState* state) {
  switch (state->managed_type()) {
    case ManagedState::MANAGED_TYPE_NETWORK:
      return "Network";
    case ManagedState::MANAGED_TYPE_DEVICE:
      return "Device";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

std::string GetLogName(const ManagedState* state) {
  if (!state) {
    return "None";
  }
  const NetworkState* network = state->AsNetworkState();
  if (network) {
    return NetworkId(network);
  }
  return state->path();
}

bool ShouldIncludeNetworkInList(const NetworkState* network_state,
                                bool configured_only,
                                bool visible_only) {
  if (configured_only && !network_state->IsInProfile()) {
    return false;
  }

  if (visible_only && !network_state->visible()) {
    return false;
  }

  if (network_state->type() == shill::kTypeWifi &&
      !network_state->tether_guid().empty()) {
    // Wi-Fi networks which are actually underlying Wi-Fi hotspots for a
    // Tether network should not be included since they should only be shown
    // to the user as Tether networks.
    return false;
  }

  return true;
}

// ManagedState entries may not have |type| set when the network is initially
// added to a list (i.e. before the initial properties are received). Use this
// wrapper anyplace where |managed| might be uninitialized.
bool TypeMatches(const ManagedState* managed, const NetworkTypePattern& type) {
  return !managed->type().empty() && managed->Matches(type);
}

}  // namespace

// Class for tracking properties that affect whether a NetworkState is active.
class NetworkStateHandler::ActiveNetworkState {
 public:
  explicit ActiveNetworkState(const NetworkState* network)
      : guid_(network->guid()),
        connection_state_(network->connection_state()),
        activation_state_(network->activation_state()),
        connect_requested_(network->connect_requested()),
        signal_strength_(network->signal_strength()),
        network_technology_(network->network_technology()),
        portal_state_(network->GetPortalState()) {}

  bool MatchesNetworkState(const NetworkState* network) {
    return guid_ == network->guid() &&
           connection_state_ == network->connection_state() &&
           activation_state_ == network->activation_state() &&
           connect_requested_ == network->connect_requested() &&
           (abs(signal_strength_ - network->signal_strength()) <
            NetworkState::kSignalStrengthChangeThreshold) &&
           network_technology_ == network->network_technology() &&
           portal_state_ == network->GetPortalState();
  }

 private:
  // Unique network identifier.
  const std::string guid_;
  // Active networks have a connected or connecting |connection_state_|, see
  // NetworkState::Is{Connected|Connecting}State.
  const std::string connection_state_;
  // Activating Cellular networks are frequently treated like connecting
  // networks in the UI, so we also track changes to Cellular activation state.
  const std::string activation_state_;
  // The connect_requested state affects 'connecting' in the UI.
  const bool connect_requested_;
  // We care about signal strength changes to active networks.
  const int signal_strength_;
  // Network technology is indicated in network icons in the UI, so we need to
  // track changes to this value.
  const std::string network_technology_;
  // Portal state changes affects the network connection state. We want to make
  // sure the network state gets updated each time the portal state changes.
  const NetworkState::PortalState portal_state_;
};

const char NetworkStateHandler::kDefaultCheckPortalList[] =
    "ethernet,wifi,cellular";

NetworkStateHandler::NetworkStateHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

NetworkStateHandler::~NetworkStateHandler() {
  // Normally Shutdown() will get called in ~NetworkHandler, however unit
  // tests do not use that class so this needs to call Shutdown when we
  // destroy the class.
  if (!did_shutdown_) {
    Shutdown();
  }
}

void NetworkStateHandler::Shutdown() {
  if (did_shutdown_) {
    return;  // May get called twice in tests.
  }
  did_shutdown_ = true;
  for (Observer& observer : observers_) {
    observer.OnShuttingDown();
  }
}

void NetworkStateHandler::InitShillPropertyHandler() {
  shill_property_handler_ =
      std::make_unique<internal::ShillPropertyHandler>(this);
  shill_property_handler_->Init();
}

void NetworkStateHandler::UpdateBlockedCellularNetworks(bool only_managed) {
  if (allow_only_policy_cellular_networks_to_connect_ == only_managed) {
    return;
  }
  allow_only_policy_cellular_networks_to_connect_ = only_managed;

  UpdateBlockedNetworksInternal(NetworkTypePattern::Cellular());
}

void NetworkStateHandler::UpdateBlockedWifiNetworks(
    bool only_managed,
    bool available_only,
    const std::vector<std::string>& blocked_hex_ssids) {
  if (allow_only_policy_wifi_networks_to_connect_ == only_managed &&
      allow_only_policy_wifi_networks_to_connect_if_available_ ==
          available_only &&
      blocked_hex_ssids_ == blocked_hex_ssids) {
    return;
  }
  allow_only_policy_wifi_networks_to_connect_ = only_managed;
  allow_only_policy_wifi_networks_to_connect_if_available_ = available_only;
  blocked_hex_ssids_ = blocked_hex_ssids;

  UpdateBlockedNetworksInternal(NetworkTypePattern::WiFi());
}

const NetworkState* NetworkStateHandler::GetAvailableManagedWifiNetwork()
    const {
  DeviceState* device =
      GetModifiableDeviceStateByType(NetworkTypePattern::WiFi());
  if (!device || !device->update_received()) {
    NET_LOG(ERROR) << "GetAvailableManagedWifiNetwork() called with no WiFi "
                   << "device available.";
    return nullptr;
  }

  const std::string& available_managed_network_path =
      device->available_managed_network_path();
  if (available_managed_network_path.empty()) {
    return nullptr;
  }
  return GetNetworkState(available_managed_network_path);
}

bool NetworkStateHandler::IsProfileNetworksLoaded() {
  return is_profile_networks_loaded_;
}

bool NetworkStateHandler::OnlyManagedWifiNetworksAllowed() const {
  return allow_only_policy_wifi_networks_to_connect_ ||
         (allow_only_policy_wifi_networks_to_connect_if_available_ &&
          GetAvailableManagedWifiNetwork());
}

void NetworkStateHandler::SyncStubCellularNetworks() {
  bool network_list_changed = AddOrRemoveStubCellularNetworks();
  if (!network_list_changed) {
    return;
  }
  SortNetworkList();
  NotifyNetworkListChanged();
}

void NetworkStateHandler::RequestTrafficCounters(
    const std::string& service_path,
    chromeos::DBusMethodCallback<base::Value> callback) {
  const NetworkState* network = GetNetworkState(service_path);

  // Return early if a network is not backed by shill, this can happen if the
  // network is a Tether network or is a non shill Cellular network.
  // see b/266972302.
  if (!network || network->IsNonProfileType()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  shill_property_handler_->RequestTrafficCounters(service_path,
                                                  std::move(callback));
}

void NetworkStateHandler::ResetTrafficCounters(
    const std::string& service_path) {
  const NetworkState* network = GetNetworkState(service_path);

  // Return early if a network is not backed by shill, this can happen if the
  // network is a Tether network or is a non shill Cellular network.
  // see b/266972302.
  if (!network || network->IsNonProfileType()) {
    return;
  }

  shill_property_handler_->ResetTrafficCounters(service_path);
}

// static
std::unique_ptr<NetworkStateHandler> NetworkStateHandler::InitializeForTest() {
  auto handler = base::WrapUnique(new NetworkStateHandler());
  handler->InitShillPropertyHandler();
  return handler;
}

void NetworkStateHandler::AddObserver(Observer* observer,
                                      const base::Location& from_here) {
  observers_.AddObserver(observer);
  device_event_log::AddEntry(
      from_here.file_name(), from_here.line_number(),
      device_event_log::LOG_TYPE_NETWORK, device_event_log::LOG_LEVEL_DEBUG,
      base::StringPrintf("NetworkStateHandler::AddObserver: 0x%p", observer));
}

void NetworkStateHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NetworkStateHandler::RemoveObserver(Observer* observer,
                                         const base::Location& from_here) {
  observers_.RemoveObserver(observer);
  device_event_log::AddEntry(
      from_here.file_name(), from_here.line_number(),
      device_event_log::LOG_TYPE_NETWORK, device_event_log::LOG_LEVEL_DEBUG,
      base::StringPrintf("NetworkStateHandler::RemoveObserver: 0x%p",
                         observer));
}

void NetworkStateHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

NetworkStateHandler::TechnologyState NetworkStateHandler::GetTechnologyState(
    const NetworkTypePattern& type) const {
  std::string technology = GetTechnologyForType(type);

  if (technology == kTypeTether) {
    return tether_technology_state_;
  }

  // If a technology is not in Shill's 'AvailableTechnologies' list, it is
  // always unavailable.
  if (!shill_property_handler_->IsTechnologyAvailable(technology)) {
    return TECHNOLOGY_UNAVAILABLE;
  }

  // Prohibited should take precedence over other states.
  if (shill_property_handler_->IsTechnologyProhibited(technology)) {
    return TECHNOLOGY_PROHIBITED;
  }

  // Disabling is a pseudostate used by the UI and takes precedence over
  // enabled.
  if (shill_property_handler_->IsTechnologyDisabling(technology)) {
    DCHECK(shill_property_handler_->IsTechnologyEnabled(technology));
    return TECHNOLOGY_DISABLING;
  }

  // Enabled and Uninitialized should be mutually exclusive. 'Enabling', which
  // is a pseudo state used by the UI, takes precedence over 'Uninitialized',
  // but not 'Enabled'.
  if (shill_property_handler_->IsTechnologyEnabled(technology)) {
    return TECHNOLOGY_ENABLED;
  }
  if (shill_property_handler_->IsTechnologyEnabling(technology)) {
    return TECHNOLOGY_ENABLING;
  }
  if (shill_property_handler_->IsTechnologyUninitialized(technology)) {
    return TECHNOLOGY_UNINITIALIZED;
  }

  // Default state is 'Available', which is equivalent to 'Initialized but not
  // enabled'.
  return TECHNOLOGY_AVAILABLE;
}

void NetworkStateHandler::SetTechnologiesEnabled(
    const NetworkTypePattern& type,
    bool enabled,
    network_handler::ErrorCallback error_callback) {
  std::vector<std::string> technologies = GetTechnologiesForType(type);
  for (const std::string& technology : technologies) {
    PerformSetTechnologyEnabled(technology, enabled, base::DoNothing(),
                                std::move(error_callback));
  }

  // Signal Device/Technology state changed.
  NotifyDeviceListChanged();
}

void NetworkStateHandler::SetTechnologyEnabled(
    const NetworkTypePattern& type,
    bool enabled,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback) {
  std::string technology = GetTechnologyForType(type);
  PerformSetTechnologyEnabled(technology, enabled, std::move(success_callback),
                              std::move(error_callback));

  // Signal Device/Technology state changed.
  NotifyDeviceListChanged();
}

void NetworkStateHandler::PerformSetTechnologyEnabled(
    const std::string& technology,
    bool enabled,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback) {
  if (technology == kTypeTether) {
    if (tether_technology_state_ != TECHNOLOGY_ENABLED &&
        tether_technology_state_ != TECHNOLOGY_AVAILABLE) {
      NET_LOG(ERROR) << "SetTechnologyEnabled() called for the Tether "
                     << "DeviceState, but the current state was: "
                     << tether_technology_state_;
      network_handler::RunErrorCallback(
          std::move(error_callback),
          NetworkConnectionHandler::kErrorEnabledOrDisabledWhenNotAvailable);
      return;
    }

    // Tether does not exist in Shill, so set |tether_technology_state_| and
    // skip the below interactions with |shill_property_handler_|.
    tether_technology_state_ =
        enabled ? TECHNOLOGY_ENABLED : TECHNOLOGY_AVAILABLE;
    return;
  }

  if (!shill_property_handler_->IsTechnologyAvailable(technology)) {
    return;
  }
  NET_LOG(USER) << "SetTechnologyEnabled " << technology << ":" << enabled;
  shill_property_handler_->SetTechnologyEnabled(technology, enabled,
                                                std::move(error_callback),
                                                std::move(success_callback));
}

void NetworkStateHandler::SetTetherTechnologyState(
    TechnologyState technology_state) {
  if (tether_technology_state_ == technology_state) {
    return;
  }

  tether_technology_state_ = technology_state;
  EnsureTetherDeviceState();

  // Signal Device/Technology state changed.
  NotifyDeviceListChanged();
}

void NetworkStateHandler::SetTetherScanState(bool is_scanning) {
  DeviceState* tether_device_state =
      GetModifiableDeviceState(kTetherDevicePath);
  if (!tether_device_state) {
    NET_LOG(ERROR) << "SetTetherScanState() called when Tether TechnologyState "
                   << "is UNAVAILABLE; cannot set scanning state.";
    return;
  }

  bool was_scanning = tether_device_state->scanning();
  tether_device_state->set_scanning(is_scanning);

  if (was_scanning && !is_scanning) {
    // If a scan was in progress but has completed, notify observers.
    NotifyScanCompleted(tether_device_state);
  } else if (!was_scanning && is_scanning) {
    // If a scan was started, notify observers.
    NotifyScanStarted(tether_device_state);
  }
}

void NetworkStateHandler::SetProhibitedTechnologies(
    const std::vector<std::string>& prohibited_technologies) {
  // Make a copy of |prohibited_technologies| since the list may be edited
  // within this function.
  std::vector<std::string> prohibited_technologies_copy =
      prohibited_technologies;

  auto it = prohibited_technologies_copy.begin();
  while (it != prohibited_technologies_copy.end()) {
    if (*it == kTypeTether) {
      // If Tether networks are prohibited, set |tether_technology_state_| and
      // remove |kTypeTether| from the list before passing it to
      // |shill_property_handler_| below. Shill does not have a concept of
      // Tether networks, so it cannot prohibit that technology type.
      tether_technology_state_ = TECHNOLOGY_PROHIBITED;
      it = prohibited_technologies_copy.erase(it);
    } else {
      ++it;
    }
  }

  shill_property_handler_->SetProhibitedTechnologies(
      prohibited_technologies_copy);
  // Signal Device/Technology state changed.
  NotifyDeviceListChanged();
}

const DeviceState* NetworkStateHandler::GetDeviceState(
    const std::string& device_path) const {
  const DeviceState* device = GetModifiableDeviceState(device_path);
  if (device && !device->update_received()) {
    NET_LOG(DEBUG) << "Device exists but update not received: " << device_path;
    return nullptr;
  }
  return device;
}

const DeviceState* NetworkStateHandler::GetDeviceStateByType(
    const NetworkTypePattern& type) const {
  const DeviceState* device = GetModifiableDeviceStateByType(type);
  if (device && !device->update_received()) {
    return nullptr;
  }
  return device;
}

bool NetworkStateHandler::GetScanningByType(
    const NetworkTypePattern& type) const {
  for (auto iter = device_list_.begin(); iter != device_list_.end(); ++iter) {
    const DeviceState* device = (*iter)->AsDeviceState();
    DCHECK(device);
    if (!device->update_received()) {
      continue;
    }
    if (device->Matches(type) && device->scanning()) {
      return true;
    }
  }
  return false;
}

const NetworkState* NetworkStateHandler::GetNetworkState(
    const std::string& service_path) const {
  const NetworkState* network = GetModifiableNetworkState(service_path);
  if (network && !network->update_received()) {
    return nullptr;
  }
  return network;
}

const NetworkState* NetworkStateHandler::DefaultNetwork() const {
  if (default_network_path_.empty()) {
    return nullptr;
  }
  return GetNetworkState(default_network_path_);
}

const NetworkState* NetworkStateHandler::ConnectedNetworkByType(
    const NetworkTypePattern& type) {
  NetworkStateList active_networks;
  GetActiveNetworkListByType(type, &active_networks);
  for (auto* network : active_networks) {
    if (network->IsConnectedState()) {
      return network;
    }
  }
  return nullptr;
}

const NetworkState* NetworkStateHandler::ConnectingNetworkByType(
    const NetworkTypePattern& type) {
  NetworkStateList active_networks;
  GetActiveNetworkListByType(type, &active_networks);
  for (auto* network : active_networks) {
    if (network->IsConnectingState()) {
      return network;
    }
  }
  return nullptr;
}

const NetworkState* NetworkStateHandler::ActiveNetworkByType(
    const NetworkTypePattern& type) {
  NetworkStateList active_networks;
  GetActiveNetworkListByType(type, &active_networks);
  if (active_networks.size() > 0) {
    return active_networks.front();
  }
  return nullptr;
}

const NetworkState* NetworkStateHandler::FirstNetworkByType(
    const NetworkTypePattern& type) {
  // Sort to ensure visible networks are listed first.
  if (!network_list_sorted_) {
    SortNetworkList();
  }

  const NetworkState* first_network = nullptr;
  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    if (!network->update_received()) {
      continue;
    }
    if (!network->visible()) {
      break;
    }
    if (network->Matches(type)) {
      first_network = network;
      break;
    }
  }

  // Active Ethernet networks are the highest priority.
  if (first_network && first_network->type() == shill::kTypeEthernet) {
    return first_network;
  }

  const NetworkState* first_tether_network =
      type.MatchesPattern(NetworkTypePattern::Tether()) &&
              !tether_network_list_.empty()
          ? tether_network_list_[0]->AsNetworkState()
          : nullptr;

  // Active Tether networks are next.
  if (first_tether_network && first_tether_network->IsConnectingOrConnected()) {
    return first_tether_network;
  }

  // Other active networks are next.
  if (first_network && first_network->IsConnectingOrConnected()) {
    return first_network;
  }

  // Non-active Tether networks are next.
  if (first_tether_network) {
    return first_tether_network;
  }

  // Other networks are last.
  return first_network;
}

void NetworkStateHandler::SetNetworkConnectRequested(
    const std::string& service_path,
    bool connect_requested) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (!network) {
    return;
  }
  network->connect_requested_ = connect_requested;
  network->shill_connect_error_.clear();
  network_list_sorted_ = false;
  OnNetworkConnectionStateChanged(network);
}

void NetworkStateHandler::SetShillConnectError(
    const std::string& service_path,
    const std::string& shill_connect_error) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (!network) {
    return;
  }
  network->shill_connect_error_ = shill_connect_error;
}

std::string NetworkStateHandler::FormattedHardwareAddressForType(
    const NetworkTypePattern& type) {
  const NetworkState* network = ConnectedNetworkByType(type);
  if (network && network->type() == kTypeTether) {
    // If this is a Tether network, get the MAC address corresponding to that
    // network instead.
    network = GetNetworkStateFromGuid(network->tether_guid());
  }
  const DeviceState* device = network ? GetDeviceState(network->device_path())
                                      : GetDeviceStateByType(type);
  if (!device || device->mac_address().empty()) {
    return std::string();
  }
  return network_util::FormattedMacAddress(device->mac_address());
}

void NetworkStateHandler::GetVisibleNetworkListByType(
    const NetworkTypePattern& type,
    NetworkStateList* list) {
  GetNetworkListByType(type, false /* configured_only */,
                       true /* visible_only */, 0 /* no limit */, list);
}

void NetworkStateHandler::GetVisibleNetworkList(NetworkStateList* list) {
  GetVisibleNetworkListByType(NetworkTypePattern::Default(), list);
}

void NetworkStateHandler::GetNetworkListByType(const NetworkTypePattern& type,
                                               bool configured_only,
                                               bool visible_only,
                                               size_t limit,
                                               NetworkStateList* list) {
  GetNetworkListByTypeImpl(type, configured_only, visible_only,
                           false /* active_only */, limit, list);
}

void NetworkStateHandler::GetActiveNetworkListByType(
    const NetworkTypePattern& type,
    NetworkStateList* list) {
  GetNetworkListByTypeImpl(type, false /* configured_only */,
                           false /* visible_only */, true /* active_only */,
                           0 /* no limit */, list);
}

void NetworkStateHandler::GetNetworkListByTypeImpl(
    const NetworkTypePattern& type,
    bool configured_only,
    bool visible_only,
    bool active_only,
    size_t limit,
    NetworkStateList* list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(list);
  list->clear();

  // If |limit| is 0, there is no limit. Simplify the calculations below by
  // setting it to the maximum size_t value.
  if (limit == 0) {
    limit = std::numeric_limits<size_t>::max();
  }

  if (!network_list_sorted_) {
    SortNetworkList();
  }

  // First, add active Tether networks.
  if (type.MatchesPattern(NetworkTypePattern::Tether())) {
    AppendTetherNetworksToList(true /* get_active */, limit, list);
  }

  // Second, add active non-Tether networks.
  for (const auto& managed : network_list_) {
    const NetworkState* network = managed.get()->AsNetworkState();
    DCHECK(network);
    if (!network->update_received() || !network->Matches(type)) {
      continue;
    }
    if (!network->IsActive()) {
      break;  // Active networks are listed first.
    }
    if (!ShouldIncludeNetworkInList(network, configured_only, visible_only)) {
      continue;
    }

    if (network->type() == shill::kTypeEthernet) {
      // Ethernet networks should always be in front.
      list->insert(list->begin(), network);
    } else {
      list->push_back(network);
    }
    if (list->size() >= limit) {
      return;
    }
  }

  if (active_only) {
    return;
  }

  // Third, add inactive Tether networks.
  if (type.MatchesPattern(NetworkTypePattern::Tether())) {
    AppendTetherNetworksToList(false /* get_active */, limit, list);
  }
  if (list->size() >= limit) {
    return;
  }

  // Fourth, add inactive non-Tether networks.
  for (const auto& managed : network_list_) {
    const NetworkState* network = managed.get()->AsNetworkState();
    DCHECK(network);
    if (!network->update_received() || !network->Matches(type)) {
      continue;
    }
    if (network->IsActive()) {
      continue;
    }
    if (!ShouldIncludeNetworkInList(network, configured_only, visible_only)) {
      continue;
    }
    list->push_back(network);
    if (list->size() >= limit) {
      return;
    }
  }
}

void NetworkStateHandler::AppendTetherNetworksToList(bool get_active,
                                                     size_t limit,
                                                     NetworkStateList* list) {
  DCHECK(list);
  DCHECK_NE(0U, limit);
  if (!IsTechnologyEnabled(NetworkTypePattern::Tether())) {
    return;
  }

  for (auto iter = tether_network_list_.begin();
       iter != tether_network_list_.end() && list->size() < limit; ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    if (network->IsConnectingOrConnected() != get_active) {
      continue;
    }
    if (!ShouldIncludeNetworkInList(network, false /* configured_only */,
                                    false /* visible_only */)) {
      continue;
    }
    list->push_back(network);
  }
}

const NetworkState* NetworkStateHandler::GetNetworkStateFromServicePath(
    const std::string& service_path,
    bool configured_only) const {
  ManagedState* managed =
      GetModifiableManagedState(&network_list_, service_path);
  if (!managed) {
    managed = GetModifiableManagedState(&tether_network_list_, service_path);
    if (!managed) {
      return nullptr;
    }
  }
  const NetworkState* network = managed->AsNetworkState();
  DCHECK(network);
  if (!network->update_received() ||
      (configured_only && !network->IsInProfile())) {
    return nullptr;
  }
  return network;
}

const NetworkState* NetworkStateHandler::GetNetworkStateFromGuid(
    const std::string& guid) const {
  DCHECK(!guid.empty());
  return GetModifiableNetworkStateFromGuid(guid);
}

void NetworkStateHandler::AddTetherNetworkState(const std::string& guid,
                                                const std::string& name,
                                                const std::string& carrier,
                                                int battery_percentage,
                                                int signal_strength,
                                                bool has_connected_to_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!guid.empty());
  DCHECK(battery_percentage >= 0 && battery_percentage <= 100);
  DCHECK(signal_strength >= 0 && signal_strength <= 100);

  if (tether_technology_state_ != TECHNOLOGY_ENABLED) {
    NET_LOG(ERROR) << "AddTetherNetworkState() called when Tether networks "
                   << "are not enabled. Cannot add NetworkState.";
    return;
  }

  // If the network already exists, do nothing.
  if (GetNetworkStateFromGuid(guid)) {
    NET_LOG(ERROR) << "AddTetherNetworkState: " << name
                   << " called with existing guid:" << guid;
    return;
  }

  // Use the GUID as the network's service path.
  std::unique_ptr<NetworkState> tether_network_state =
      std::make_unique<NetworkState>(guid /* path */);

  tether_network_state->set_name(name);
  tether_network_state->set_type(kTypeTether);
  tether_network_state->SetGuid(guid);
  tether_network_state->set_visible(true);
  tether_network_state->set_update_received();
  tether_network_state->set_update_requested(false);
  tether_network_state->set_connectable(true);
  tether_network_state->set_tether_carrier(carrier);
  tether_network_state->set_battery_percentage(battery_percentage);
  tether_network_state->set_tether_has_connected_to_host(has_connected_to_host);
  tether_network_state->set_signal_strength(signal_strength);

  tether_network_list_.push_back(std::move(tether_network_state));
  network_list_sorted_ = false;

  NotifyNetworkListChanged();
}

bool NetworkStateHandler::UpdateTetherNetworkProperties(
    const std::string& guid,
    const std::string& carrier,
    int battery_percentage,
    int signal_strength) {
  if (tether_technology_state_ != TECHNOLOGY_ENABLED) {
    NET_LOG(ERROR) << "UpdateTetherNetworkProperties() called when Tether "
                   << "networks are not enabled. Cannot update.";
    return false;
  }

  NetworkState* tether_network_state = GetModifiableNetworkStateFromGuid(guid);
  if (!tether_network_state) {
    NET_LOG(ERROR) << "UpdateTetherNetworkProperties(): No NetworkState for "
                   << "Tether network with GUID \"" << guid << "\".";
    return false;
  }

  tether_network_state->set_tether_carrier(carrier);
  tether_network_state->set_battery_percentage(battery_percentage);
  tether_network_state->set_signal_strength(signal_strength);
  network_list_sorted_ = false;

  NotifyNetworkPropertiesUpdated(tether_network_state);
  if (tether_network_state->IsConnectingOrConnected()) {
    NotifyIfActiveNetworksChanged();
  }
  return true;
}

bool NetworkStateHandler::SetTetherNetworkHasConnectedToHost(
    const std::string& guid) {
  NetworkState* tether_network_state = GetModifiableNetworkStateFromGuid(guid);
  if (!tether_network_state) {
    NET_LOG(ERROR) << "SetTetherNetworkHasConnectedToHost(): No NetworkState "
                   << "for Tether network with GUID \"" << guid << "\".";
    return false;
  }

  if (tether_network_state->tether_has_connected_to_host()) {
    return false;
  }

  tether_network_state->set_tether_has_connected_to_host(true);
  network_list_sorted_ = false;

  NotifyNetworkPropertiesUpdated(tether_network_state);
  return true;
}

bool NetworkStateHandler::RemoveTetherNetworkState(const std::string& guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!notifying_network_observers_);
  for (auto iter = tether_network_list_.begin();
       iter != tether_network_list_.end(); ++iter) {
    if (iter->get()->AsNetworkState()->guid() == guid) {
      const NetworkState* tether_network = iter->get()->AsNetworkState();
      bool was_active = tether_network->IsConnectingOrConnected();
      NetworkState* wifi_network =
          GetModifiableNetworkStateFromGuid(tether_network->tether_guid());
      if (wifi_network) {
        wifi_network->set_tether_guid(std::string());
      }
      tether_network_list_.erase(iter);

      if (was_active) {
        NotifyIfActiveNetworksChanged();
      }
      NotifyNetworkListChanged();
      return true;
    }
  }
  return false;
}

bool NetworkStateHandler::DisassociateTetherNetworkStateFromWifiNetwork(
    const std::string& tether_network_guid) {
  NetworkState* tether_network_state =
      GetModifiableNetworkStateFromGuid(tether_network_guid);

  if (!tether_network_state) {
    NET_LOG(ERROR) << "DisassociateTetherNetworkStateWithWifiNetwork(): Tether "
                   << "network with ID " << tether_network_guid
                   << " not registered; could not remove association.";
    return false;
  }

  std::string wifi_network_guid = tether_network_state->tether_guid();
  NetworkState* wifi_network_state =
      GetModifiableNetworkStateFromGuid(wifi_network_guid);

  if (!wifi_network_state) {
    NET_LOG(ERROR) << "DisassociateTetherNetworkStateWithWifiNetwork(): Wi-Fi "
                   << "network with ID " << wifi_network_guid
                   << " not registered; could not remove association.";
    return false;
  }

  if (wifi_network_state->tether_guid().empty() &&
      tether_network_state->tether_guid().empty()) {
    return true;
  }

  wifi_network_state->set_tether_guid(std::string());
  tether_network_state->set_tether_guid(std::string());
  network_list_sorted_ = false;

  NotifyNetworkPropertiesUpdated(wifi_network_state);
  NotifyNetworkPropertiesUpdated(tether_network_state);

  return true;
}

bool NetworkStateHandler::AssociateTetherNetworkStateWithWifiNetwork(
    const std::string& tether_network_guid,
    const std::string& wifi_network_guid) {
  if (tether_technology_state_ != TECHNOLOGY_ENABLED) {
    NET_LOG(ERROR) << "AssociateTetherNetworkStateWithWifiNetwork() called "
                   << "when Tether networks are not enabled. Cannot "
                   << "associate.";
    return false;
  }

  NetworkState* tether_network_state =
      GetModifiableNetworkStateFromGuid(tether_network_guid);
  if (!tether_network_state) {
    NET_LOG(ERROR) << "Tether network does not exist: " << tether_network_guid;
    return false;
  }
  if (!NetworkTypePattern::Tether().MatchesType(tether_network_state->type())) {
    NET_LOG(ERROR) << "Network is not a Tether network: "
                   << tether_network_guid;
    return false;
  }

  NetworkState* wifi_network_state =
      GetModifiableNetworkStateFromGuid(wifi_network_guid);
  if (!wifi_network_state) {
    NET_LOG(ERROR) << "Wi-Fi Network does not exist: " << wifi_network_guid;
    return false;
  }
  if (!NetworkTypePattern::WiFi().MatchesType(wifi_network_state->type())) {
    NET_LOG(ERROR) << "Network is not a W-Fi network: "
                   << NetworkId(wifi_network_state);
    return false;
  }

  if (wifi_network_state->tether_guid() == tether_network_guid &&
      tether_network_state->tether_guid() == wifi_network_guid) {
    return true;
  }

  tether_network_state->set_tether_guid(wifi_network_guid);
  wifi_network_state->set_tether_guid(tether_network_guid);
  network_list_sorted_ = false;

  NotifyNetworkPropertiesUpdated(wifi_network_state);
  NotifyNetworkPropertiesUpdated(tether_network_state);

  return true;
}

void NetworkStateHandler::SetTetherNetworkStateDisconnected(
    const std::string& guid) {
  SetTetherNetworkStateConnectionState(guid, shill::kStateIdle);
}

void NetworkStateHandler::SetTetherNetworkStateConnecting(
    const std::string& guid) {
  // The default network should only be set if there currently is no default
  // network. Otherwise, the default network should not change unless the
  // connection completes successfully and the newly-connected network is
  // prioritized higher than the existing default network. Note that, in
  // general, a connected Ethernet network is still considered the default
  // network even if a Tether or Wi-Fi network becomes connected.
  if (default_network_path_.empty()) {
    NET_LOG(EVENT) << "Connecting to Tether network when there is currently no "
                   << "default network; setting as new default network. GUID: "
                   << guid;
    SetDefaultNetworkValues(guid, /*metered=*/true);
  }

  SetTetherNetworkStateConnectionState(guid, shill::kStateConfiguration);
}

void NetworkStateHandler::SetTetherNetworkStateConnected(
    const std::string& guid) {
  // Being connected implies that AssociateTetherNetworkStateWithWifiNetwork()
  // was already called, so ensure that the association is still intact.
  // TODO(b/278966899): Promote this to a CHECK.
  DCHECK(GetNetworkStateFromGuid(GetNetworkStateFromGuid(guid)->tether_guid())
             ->tether_guid() == guid);

  // At this point, there should be a default network set.
  // TODO(b/279047073): We can hit this due to a race between
  // `SetTetherNetworkStateConnected` and `DefaultNetworkServiceChange`.
  DCHECK(!default_network_path_.empty());

  SetTetherNetworkStateConnectionState(guid, shill::kStateOnline);
}

void NetworkStateHandler::SetTetherNetworkStateConnectionState(
    const std::string& guid,
    const std::string& connection_state) {
  NetworkState* tether_network_state = GetModifiableNetworkStateFromGuid(guid);
  if (!tether_network_state) {
    NET_LOG(ERROR) << "SetTetherNetworkStateConnectionState: Tether network "
                   << "not found: " << guid;
    return;
  }

  DCHECK(
      NetworkTypePattern::Tether().MatchesType(tether_network_state->type()));

  std::string prev_connection_state = tether_network_state->connection_state();
  tether_network_state->SetConnectionState(connection_state);
  network_list_sorted_ = false;

  if (ConnectionStateChanged(tether_network_state, prev_connection_state)) {
    NET_LOG(EVENT) << "Changing connection state for Tether network with GUID "
                   << guid << ". Old state: " << prev_connection_state << ", "
                   << "New state: " << connection_state;
    if (!tether_network_state->IsConnectingOrConnected() &&
        tether_network_state->path() == default_network_path_) {
      SetDefaultNetworkValues(/*path=*/std::string(), /*metered=*/false);
      NotifyDefaultNetworkChanged(kReasonTether);
    }
    OnNetworkConnectionStateChanged(tether_network_state);
    NotifyNetworkPropertiesUpdated(tether_network_state);
  }
}

void NetworkStateHandler::EnsureTetherDeviceState() {
  bool should_be_present =
      tether_technology_state_ != TechnologyState::TECHNOLOGY_UNAVAILABLE;

  for (auto it = device_list_.begin(); it < device_list_.end(); ++it) {
    std::string path = (*it)->path();
    if (path == kTetherDevicePath) {
      // If the Tether DeviceState is in the list and it should not be, remove
      // it and return. If it is in the list and it should be, the list is
      // already valid, so return without removing it.
      if (!should_be_present) {
        device_list_.erase(it);
      }
      return;
    }
  }

  if (!should_be_present) {
    // If the Tether DeviceState was not in the list and it should not be, the
    // list is already valid, so return.
    return;
  }

  // The Tether DeviceState is not present in the list, but it should be. Since
  // Tether networks are not recognized by Shill, they will never receive an
  // update, so set properties on the state here.
  std::unique_ptr<ManagedState> tether_device_state = ManagedState::Create(
      ManagedState::ManagedType::MANAGED_TYPE_DEVICE, kTetherDevicePath);
  tether_device_state->set_update_received();
  tether_device_state->set_update_requested(false);
  tether_device_state->set_name(kTetherDeviceName);
  tether_device_state->set_type(kTypeTether);

  device_list_.push_back(std::move(tether_device_state));
}

bool NetworkStateHandler::UpdateBlockedByPolicy(NetworkState* network) const {
  bool is_wifi_type = TypeMatches(network, NetworkTypePattern::WiFi());
  bool is_cellular_type = TypeMatches(network, NetworkTypePattern::Cellular());
  if (!is_wifi_type && !is_cellular_type) {
    return false;
  }

  bool prev_blocked_by_policy = network->blocked_by_policy();
  bool blocked_by_policy = false;
  if (is_wifi_type) {
    blocked_by_policy =
        !network->IsManagedByPolicy() &&
        (OnlyManagedWifiNetworksAllowed() ||
         base::Contains(blocked_hex_ssids_, network->GetHexSsid()));
  } else {
    blocked_by_policy = !network->IsManagedByPolicy() &&
                        allow_only_policy_cellular_networks_to_connect_;
  }
  network->set_blocked_by_policy(blocked_by_policy);
  return prev_blocked_by_policy != blocked_by_policy;
}

void NetworkStateHandler::UpdateManagedWifiNetworkAvailable() {
  DeviceState* device =
      GetModifiableDeviceStateByType(NetworkTypePattern::WiFi());
  if (!device || !device->update_received()) {
    return;  // May be null in tests.
  }

  const std::string prev_available_managed_network_path =
      device->available_managed_network_path();
  std::string available_managed_network_path;

  NetworkStateHandler::NetworkStateList networks;
  GetNetworkListByType(NetworkTypePattern::WiFi(), true, true, 0, &networks);
  for (const NetworkState* network : networks) {
    if (network->IsManagedByPolicy()) {
      available_managed_network_path = network->path();
      break;
    }
  }

  if (prev_available_managed_network_path != available_managed_network_path) {
    device->set_available_managed_network_path(available_managed_network_path);
    UpdateBlockedNetworksInternal(NetworkTypePattern::WiFi());
    NotifyDevicePropertiesUpdated(device);
  }
}

void NetworkStateHandler::UpdateBlockedNetworksInternal(
    const NetworkTypePattern& network_type) {
  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    if (!TypeMatches(network, network_type)) {
      continue;
    }
    if (UpdateBlockedByPolicy(network)) {
      NotifyNetworkPropertiesUpdated(network);
    }
  }
}

void NetworkStateHandler::GetDeviceList(DeviceStateList* list) const {
  GetDeviceListByType(NetworkTypePattern::Default(), list);
}

void NetworkStateHandler::GetDeviceListByType(const NetworkTypePattern& type,
                                              DeviceStateList* list) const {
  DCHECK(list);
  list->clear();

  for (auto iter = device_list_.begin(); iter != device_list_.end(); ++iter) {
    const DeviceState* device = (*iter)->AsDeviceState();
    DCHECK(device);
    if (device->update_received() && device->Matches(type)) {
      list->push_back(device);
    }
  }
}

void NetworkStateHandler::RequestScan(const NetworkTypePattern& type) {
  NET_LOG(USER) << "RequestScan: " << type.ToDebugString();
  if (type.MatchesPattern(NetworkTypePattern::WiFi())) {
    if (IsTechnologyEnabled(NetworkTypePattern::WiFi())) {
      shill_property_handler_->RequestScanByType(shill::kTypeWifi);
    } else if (type.Equals(NetworkTypePattern::WiFi())) {
      return;  // Skip notify if disabled and wifi only requested.
    }
  }

  if (type.Equals(NetworkTypePattern::Cellular())) {
    // Only request a Cellular scan if Cellular is requested explicitly.
    if (IsTechnologyEnabled(NetworkTypePattern::Cellular())) {
      shill_property_handler_->RequestScanByType(shill::kTypeCellular);
    } else {
      return;  // Skip notify if disabled and cellular only requested.
    }
  }

  // Note: for Tether we initiate the scan in the observer.
  NotifyScanRequested(type);
}

void NetworkStateHandler::RequestUpdateForNetwork(
    const std::string& service_path) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (network) {
    // Do not request properties for networks which are not backed by Shill.
    if (network->IsNonProfileType()) {
      return;
    }
    // Do not request properties if a condition has already triggered a request.
    if (network->update_requested()) {
      return;
    }
    network->set_update_requested(true);
    NET_LOG(EVENT) << "RequestUpdate for: " << NetworkId(network);
  } else {
    NET_LOG(EVENT) << "RequestUpdate for: " << NetworkPathId(service_path);
  }
  shill_property_handler_->RequestProperties(ManagedState::MANAGED_TYPE_NETWORK,
                                             service_path);
  network_service_paths_with_stale_properties_.erase(service_path);
}

void NetworkStateHandler::RequestUpdateForDevice(
    const std::string& device_path) {
  DeviceState* device = GetModifiableDeviceState(device_path);
  if (!device) {
    return;
  }

  device->set_update_requested(true);
  NET_LOG(EVENT) << "Request update for device path: " << device_path;
  shill_property_handler_->RequestProperties(ManagedState::MANAGED_TYPE_DEVICE,
                                             device_path);
  device_paths_with_stale_properties_.erase(device_path);
}

void NetworkStateHandler::SendUpdateNotificationForNetwork(
    const std::string& service_path) {
  const NetworkState* network = GetNetworkState(service_path);
  if (!network) {
    return;
  }
  NotifyNetworkPropertiesUpdated(network);
}

void NetworkStateHandler::ClearLastErrorForNetwork(
    const std::string& service_path) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (network) {
    network->ClearError();
  }
}

void NetworkStateHandler::SetWakeOnLanEnabled(bool enabled) {
  NET_LOG(EVENT) << "SetWakeOnLanEnabled: " << enabled;
  shill_property_handler_->SetWakeOnLanEnabled(enabled);
}

void NetworkStateHandler::SetHostname(const std::string& hostname) {
  NET_LOG(EVENT) << "SetHostname";
  shill_property_handler_->SetHostname(hostname);
}

void NetworkStateHandler::SetNetworkThrottlingStatus(
    bool enabled,
    uint32_t upload_rate_kbits,
    uint32_t download_rate_kbits) {
  if (enabled) {
    NET_LOG(EVENT) << "SetNetworkThrottlingStatus: Enabled: "
                   << upload_rate_kbits << ", " << download_rate_kbits;
  } else {
    NET_LOG(EVENT) << "SetNetworkThrottlingStatus: Disabled.";
  }
  shill_property_handler_->SetNetworkThrottlingStatus(
      enabled, upload_rate_kbits, download_rate_kbits);
}

void NetworkStateHandler::SetFastTransitionStatus(bool enabled) {
  NET_LOG(USER) << "SetFastTransitionStatus: " << enabled;
  shill_property_handler_->SetFastTransitionStatus(enabled);
}

void NetworkStateHandler::RequestPortalDetection() {
  const NetworkState* default_network = DefaultNetwork();
  if (!default_network) {
    NET_LOG(DEBUG) << "RequestPortalDetection skipped, no default network.";
    return;
  }
  if (default_network->IsOnline()) {
    NET_LOG(DEBUG) << "RequestPortalDetection skipped for online network: "
                   << NetworkId(default_network);
    return;
  }
  NET_LOG(USER) << "RequestPortalDetection for " << NetworkId(default_network);
  shill_property_handler_->RequestPortalDetection(default_network_path_);
}

const NetworkState* NetworkStateHandler::GetEAPForEthernet(
    const std::string& service_path,
    bool connected_only) {
  const NetworkState* network = GetNetworkState(service_path);
  if (!network) {
    NET_LOG(ERROR) << "GetEAPForEthernet: Unknown service: "
                   << NetworkPathId(service_path);
    return nullptr;
  }
  if (network->type() != shill::kTypeEthernet) {
    NET_LOG(ERROR) << "GetEAPForEthernet: Not Ethernet: " << NetworkId(network);
    return nullptr;
  }
  if (connected_only) {
    if (!network->IsConnectedState()) {
      NET_LOG(DEBUG) << "GetEAPForEthernet: Not connected.";
      return nullptr;
    }

    // The same EAP service is shared for all ethernet services/devices.
    // However EAP is used/enabled per device and only if the connection was
    // successfully established.
    const DeviceState* device = GetDeviceState(network->device_path());
    if (!device) {
      NET_LOG(ERROR) << "GetEAPForEthernet: Unknown device "
                     << network->device_path()
                     << " for connected ethernet service: "
                     << NetworkId(network);
      return nullptr;
    }
    if (!device->eap_authentication_completed()) {
      NET_LOG(DEBUG) << "GetEAPForEthernet: EAP Authenticaiton not completed.";
      return nullptr;
    }
  }

  NetworkStateList list;
  GetNetworkListByType(NetworkTypePattern::Primitive(shill::kTypeEthernetEap),
                       true /* configured_only */, false /* visible_only */,
                       1 /* limit */, &list);
  if (list.empty()) {
    if (connected_only) {
      NET_LOG(ERROR)
          << "GetEAPForEthernet: Connected using EAP but no EAP service found: "
          << NetworkId(network);
    }
    return nullptr;
  }
  return list.front();
}

void NetworkStateHandler::SetErrorForTest(const std::string& service_path,
                                          const std::string& error) {
  NetworkState* network_state = GetModifiableNetworkState(service_path);
  if (!network_state) {
    NET_LOG(ERROR) << "No matching NetworkState for: "
                   << NetworkPathId(service_path);
    return;
  }
  network_state->last_error_ = error;
}

void NetworkStateHandler::SetDeviceStateUpdatedForTest(
    const std::string& device_path) {
  DeviceState* device = GetModifiableDeviceState(device_path);
  DCHECK(device);
  device->set_update_received();
}

//------------------------------------------------------------------------------
// ShillPropertyHandler::Delegate overrides

void NetworkStateHandler::UpdateManagedList(ManagedState::ManagedType type,
                                            const base::Value::List& entries) {
  CHECK(!notifying_network_observers_);

  ManagedStateList* managed_list = GetManagedList(type);
  NET_LOG(EVENT) << "UpdateManagedList: " << ManagedState::TypeToString(type)
                 << ": " << entries.size();
  // Create a map of existing entries. Assumes all entries in |managed_list|
  // are unique.
  std::map<std::string, std::unique_ptr<ManagedState>> managed_map;
  for (auto& item : *managed_list) {
    std::string path = item->path();
    DCHECK(!base::Contains(managed_map, path));
    managed_map[path] = std::move(item);
  }
  // Clear the list (objects are temporarily owned by managed_map).
  managed_list->clear();
  // Updates managed_list and request updates for new entries.
  std::set<std::string> list_entries;
  for (const auto& iter : entries) {
    const std::string* path = iter.GetIfString();
    if (!path) {
      continue;
    }
    if (!path || (*path).empty() || *path == shill::kFlimflamServicePath) {
      NET_LOG(ERROR) << "Bad path in type " << type << " Path: " << *path;
      continue;
    }
    auto found = managed_map.find(*path);
    if (found == managed_map.end()) {
      if (list_entries.count(*path) != 0) {
        NET_LOG(ERROR) << "Duplicate entry in list for " << *path;
        continue;
      }
      managed_list->push_back(ManagedState::Create(type, *path));
    } else {
      managed_list->push_back(std::move(found->second));
      managed_map.erase(found);
    }
    list_entries.insert(*path);
  }

  if (type == ManagedState::ManagedType::MANAGED_TYPE_DEVICE) {
    // Also move the Tether DeviceState if it exists. This will not happen as
    // part of the loop above since |entries| will never contain the Tether
    // path.
    auto iter = managed_map.find(kTetherDevicePath);
    if (iter != managed_map.end()) {
      managed_list->push_back(std::move(iter->second));
      managed_map.erase(iter);
    }
  }

  UpdateManagedWifiNetworkAvailable();
  UpdateBlockedCellularNetworks();
  if (type != ManagedState::ManagedType::MANAGED_TYPE_NETWORK) {
    return;
  }

  // Non-Shill services are added in Chrome and is not present in |entries|.
  // Add these services back to managed_list.
  for (auto iter = managed_map.begin(); iter != managed_map.end();) {
    NetworkState* network = iter->second->AsNetworkState();
    if (!network->IsNonShillCellularNetwork()) {
      iter++;
      continue;
    }
    managed_list->push_back(std::move(iter->second));
    iter = managed_map.erase(iter);
  }

  // Network list is explicitly sorted in ManagedListChanged() which is notified
  // after this method. But this ensures that any intervening calls to
  // GetNetworkList* methods will use the sorted list.
  network_list_sorted_ = false;

  // Remove associations Tether NetworkStates had with now removed Wi-Fi
  // NetworkStates.
  for (auto& iter : managed_map) {
    ManagedState* managed = iter.second.get();
    if (!TypeMatches(managed, NetworkTypePattern::WiFi())) {
      continue;
    }
    NetworkState* tether_network = GetModifiableNetworkStateFromGuid(
        managed->AsNetworkState()->tether_guid());
    if (tether_network) {
      tether_network->set_tether_guid(std::string());
    }
  }
}

void NetworkStateHandler::UpdateBlockedCellularNetworks() {
  DeviceState* device =
      GetModifiableDeviceStateByType(NetworkTypePattern::Cellular());
  if (!device || !device->update_received()) {
    return;  // May be null in tests.
  }

  UpdateBlockedNetworksInternal(NetworkTypePattern::Cellular());
}

void NetworkStateHandler::ProfileListChanged(
    const base::Value::List& profile_list) {
  NET_LOG(EVENT) << "ProfileListChanged. Re-Requesting Network Properties";
  ProcessIsUserLoggedIn(profile_list);
  for (ManagedStateList::iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);

    // Do not request properties for networks which are not backed by Shill.
    if (network->IsNonProfileType()) {
      continue;
    }

    shill_property_handler_->RequestProperties(
        ManagedState::MANAGED_TYPE_NETWORK, network->path());
  }
}

void NetworkStateHandler::UpdateManagedStateProperties(
    ManagedState::ManagedType type,
    const std::string& path,
    const base::Value::Dict& properties) {
  ManagedStateList* managed_list = GetManagedList(type);
  ManagedState* managed = GetModifiableManagedState(managed_list, path);
  if (!managed) {
    // The network has been removed from the list of networks.
    NET_LOG(DEBUG) << "UpdateManagedStateProperties: Not found: " << path;
    return;
  }
  managed->set_update_received();

  NET_LOG(EVENT) << GetManagedStateLogType(managed)
                 << " Properties Received: " << GetLogName(managed);

  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    UpdateNetworkStateProperties(managed->AsNetworkState(), properties);
    managed->set_update_requested(false);
    if (network_service_paths_with_stale_properties_.find(path) !=
        network_service_paths_with_stale_properties_.end()) {
      RequestUpdateForNetwork(path);
    }
    return;
  }

  // Device
  for (const auto iter : properties) {
    managed->PropertyChanged(iter.first, iter.second);
  }
  managed->InitialPropertiesReceived(properties);
  managed->set_update_requested(false);

  if (device_paths_with_stale_properties_.find(path) !=
      device_paths_with_stale_properties_.end()) {
    RequestUpdateForDevice(path);
  }
}

void NetworkStateHandler::UpdateNetworkStateProperties(
    NetworkState* network,
    const base::Value::Dict& properties) {
  DCHECK(network);
  bool network_property_updated = false;
  std::string prev_connection_state = network->connection_state();
  bool metered = false;
  bool had_icccid_before_update = !network->iccid().empty();
  for (const auto iter : properties) {
    if (network->PropertyChanged(iter.first, iter.second)) {
      network_property_updated = true;
    }
    if (iter.first == shill::kMeteredProperty) {
      metered = iter.second.is_bool() && iter.second.GetBool();
    }
  }
  if (network->path() == default_network_path_) {
    default_network_is_metered_ = metered && network->IsConnectedState();
  }

  if (network->Matches(NetworkTypePattern::WiFi() |
                       NetworkTypePattern::Cellular())) {
    network_property_updated |= UpdateBlockedByPolicy(network);
  }
  network_property_updated |= network->InitialPropertiesReceived(properties);

  UpdateGuid(network);

  network_list_sorted_ = false;

  if (network->Matches(NetworkTypePattern::Cellular())) {
    HandleCellularNetworkUpdateReceived(network, had_icccid_before_update);
  }

  // Notify observers of NetworkState changes.
  if (network_property_updated || network->update_requested()) {
    // Signal connection state changed after all properties have been updated.
    if (ConnectionStateChanged(network, prev_connection_state)) {
      // Also notifies that the default network changed if this is the default.
      OnNetworkConnectionStateChanged(network);
    } else if (network->path() == default_network_path_ &&
               network->IsActive()) {
      // Always notify that the default network changed for a complete update.
      NET_LOG(DEBUG) << "UpdateNetworkStateProperties for default: "
                     << NetworkId(network);
      NotifyDefaultNetworkChanged(kReasonUpdate);
    }
    NotifyNetworkPropertiesUpdated(network);
  }
}

void NetworkStateHandler::UpdateNetworkServiceProperty(
    const std::string& service_path,
    const std::string& key,
    const base::Value& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_NET_LOG_IF_SLOW();
  bool changed = false;
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (!network) {
    return;
  }

  // When shill::kProfileProperty is updated, the `ProfileListChanged` already
  // requests the latest network properties, so we don't need to request again.
  if (!network->update_received() && key != shill::kProfileProperty) {
    network_service_paths_with_stale_properties_.insert(service_path);
    return;
  }

  std::string prev_connection_state = network->connection_state();
  std::string prev_profile_path = network->profile_path();
  bool had_icccid_before_update = !network->iccid().empty();
  changed |= network->PropertyChanged(key, value);
  changed |= UpdateBlockedByPolicy(network);
  if (!changed) {
    return;
  }

  // If added or removed from a Profile, request a full update so that a
  // NetworkState gets created.
  bool request_update = prev_profile_path != network->profile_path();
  bool sort_networks = false;
  bool notify_default = network->path() == default_network_path_;
  bool notify_connection_state = false;
  bool notify_active = false;

  if (key == shill::kStateProperty || key == shill::kVisibleProperty) {
    network_list_sorted_ = false;
    if (ConnectionStateChanged(network, prev_connection_state)) {
      notify_connection_state = true;
      notify_active = true;
      if (notify_default) {
        notify_default = VerifyDefaultNetworkConnectionStateChange(network);
      }
      // If the default network connection state changed, sort networks now
      // and ensure that a default cellular network exists.
      if (notify_default) {
        sort_networks = true;
      }

      // If the connection state changes, other properties such as IPConfig
      // may have changed, so request a full update.
      request_update = true;
    }
  } else if (key == shill::kActivationStateProperty) {
    // Activation state may affect "connecting" state in the UI.
    notify_connection_state = true;
    network_list_sorted_ = false;
  }

  if (network->Matches(NetworkTypePattern::Cellular())) {
    HandleCellularNetworkUpdateReceived(network, had_icccid_before_update);
  }

  if (request_update) {
    RequestUpdateForNetwork(service_path);
    notify_default = false;  // Notify will occur when properties are received.
  }

  const std::string* value_str = value.GetIfString();
  if (key == shill::kSignalStrengthProperty || key == shill::kWifiBSsid ||
      key == shill::kWifiFrequency ||
      key == shill::kWifiFrequencyListProperty ||
      key == shill::kNetworkTechnologyProperty ||
      (key == shill::kDeviceProperty && value_str && *value_str == "/")) {
    // Uninteresting update. This includes 'Device' property changes to "/"
    // (occurs just before a service is removed).
    // For non active networks do not log or send any notifications.
    if (!network->IsActive()) {
      return;
    }
    // Otherwise do not trigger 'default network changed'.
    notify_default = false;
    // Notify signal strength and network technology changes for active
    // networks.
    if (key == shill::kSignalStrengthProperty ||
        key == shill::kNetworkTechnologyProperty) {
      notify_active = true;
    }
  }

  LogPropertyUpdated(network, key, value);
  if (notify_connection_state) {
    NotifyNetworkConnectionStateChanged(network);
  }
  if (notify_default) {
    std::stringstream logstream;
    logstream << std::string(kReasonUpdate) << ":" << key << "=" << value;
    NotifyDefaultNetworkChanged(logstream.str());
  }
  if (notify_active) {
    NotifyIfActiveNetworksChanged();
  }
  NotifyNetworkPropertiesUpdated(network);
  if (sort_networks) {
    bool network_list_changed = AddOrRemoveStubCellularNetworks();
    SortNetworkList();
    if (network_list_changed) {
      NotifyNetworkListChanged();
    }
  }
}

void NetworkStateHandler::UpdateDeviceProperty(const std::string& device_path,
                                               const std::string& key,
                                               const base::Value& value) {
  SCOPED_NET_LOG_IF_SLOW();
  DeviceState* device = GetModifiableDeviceState(device_path);
  if (!device) {
    return;
  }

  if (!device->update_received()) {
    device_paths_with_stale_properties_.insert(device_path);
    return;
  }

  const bool was_scanning = device->scanning();
  if (!device->PropertyChanged(key, value)) {
    return;
  }

  LogPropertyUpdated(device, key, value);
  NotifyDevicePropertiesUpdated(device);

  if (key == shill::kScanningProperty && was_scanning != device->scanning()) {
    if (device->scanning()) {
      NotifyScanStarted(device);
    } else {
      NotifyScanCompleted(device);
    }

    if (device->type() == shill::kTypeWifi && !device->scanning()) {
      UpdateManagedWifiNetworkAvailable();
    }
    if (device->type() == shill::kTypeCellular && !device->scanning()) {
      UpdateBlockedCellularNetworks();
    }
  }
  if (key == shill::kEapAuthenticationCompletedProperty) {
    // Notify a change for each Ethernet service using this device.
    NetworkStateList ethernet_services;
    GetNetworkListByType(NetworkTypePattern::Ethernet(),
                         false /* configured_only */, false /* visible_only */,
                         0 /* no limit */, &ethernet_services);
    for (NetworkStateList::const_iterator it = ethernet_services.begin();
         it != ethernet_services.end(); ++it) {
      const NetworkState* ethernet_service = *it;
      if (ethernet_service->update_received() ||
          ethernet_service->device_path() != device->path()) {
        continue;
      }
      RequestUpdateForNetwork(ethernet_service->path());
    }
  }
  if (key == shill::kSIMSlotInfoProperty) {
    // Change in SIM Slot info can result in changes to stub cellular services.
    SyncStubCellularNetworks();
  }
}

void NetworkStateHandler::UpdateIPConfigProperties(
    ManagedState::ManagedType type,
    const std::string& path,
    const std::string& ip_config_path,
    base::Value::Dict properties) {
  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    NetworkState* network = GetModifiableNetworkState(path);
    if (!network) {
      return;
    }
    network->IPConfigPropertiesChanged(properties);
    NotifyNetworkPropertiesUpdated(network);
    if (network->path() == default_network_path_) {
      NotifyDefaultNetworkChanged(kReasonUpdateIPConfig);
    }
    if (network->IsActive()) {
      NotifyIfActiveNetworksChanged();
    }
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    DeviceState* device = GetModifiableDeviceState(path);
    if (!device) {
      return;
    }
    device->IPConfigPropertiesChanged(ip_config_path, std::move(properties));
    NotifyDevicePropertiesUpdated(device);
    if (!default_network_path_.empty()) {
      const NetworkState* default_network =
          GetNetworkState(default_network_path_);
      if (default_network && default_network->device_path() == path) {
        NotifyNetworkPropertiesUpdated(default_network);
        NotifyDefaultNetworkChanged(kReasonUpdateDeviceIPConfig);
      }
    }
  }
}

void NetworkStateHandler::CheckPortalListChanged(
    const std::string& check_portal_list) {
  check_portal_list_ = check_portal_list;
}

void NetworkStateHandler::HostnameChanged(const std::string& hostname) {
  NET_LOG(EVENT) << "HostnameChanged";
  hostname_ = hostname;
  for (Observer& observer : observers_) {
    observer.HostnameChanged(hostname);
  }
}

void NetworkStateHandler::TechnologyListChanged() {
  // Eventually we would like to replace Technology state with Device state.
  // For now, treat technology state changes as device list changes.
  NotifyDeviceListChanged();

  // Stub cellular networks can be affected by cellular technology state.
  SyncStubCellularNetworks();
}

void NetworkStateHandler::ManagedStateListChanged(
    ManagedState::ManagedType type) {
  SCOPED_NET_LOG_IF_SLOW();
  switch (type) {
    case ManagedState::MANAGED_TYPE_NETWORK:
      AddOrRemoveStubCellularNetworks();
      SortNetworkList();
      NotifyIfActiveNetworksChanged();
      NotifyNetworkListChanged();
      UpdateBlockedCellularNetworks();
      UpdateManagedWifiNetworkAvailable();
      // ManagedStateListChanged only gets executed if all pending updates have
      // completed. Profile networks are loaded if a user is logged in and all
      // pending updates are complete.
      is_profile_networks_loaded_ = is_user_logged_in_;
      return;
    case ManagedState::MANAGED_TYPE_DEVICE:
      std::string devices;
      for (auto iter = device_list_.begin(); iter != device_list_.end();
           ++iter) {
        if (iter != device_list_.begin()) {
          devices += ", ";
        }
        devices += (*iter)->name();
      }
      NET_LOG(EVENT) << "DeviceList: " << devices;
      NotifyDeviceListChanged();
      // A change to the device list may affect the default Cellular network.
      SyncStubCellularNetworks();
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void NetworkStateHandler::SortNetworkList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tether_sort_delegate_) {
    tether_sort_delegate_->SortTetherNetworkList(&tether_network_list_);
  }

  // Note: usually active networks will precede inactive networks, however
  // this may briefly be untrue during state transitions (e.g. a network may
  // transition to idle before the list is updated). Also separate inactive
  // Mobile and VPN networks (see below).
  ManagedStateList active, non_wifi_visible, wifi_visible, hidden, new_networks;
  for (ManagedStateList::iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    // NetworkState entries are created when they appear in the list, but the
    // details are not populated until an update is received.
    if (!network->update_received()) {
      new_networks.push_back(std::move(*iter));
      continue;
    }
    if (network->IsActive()) {
      active.push_back(std::move(*iter));
      continue;
    }
    if (!network->visible()) {
      hidden.push_back(std::move(*iter));
      continue;
    }
    if (NetworkTypePattern::WiFi().MatchesType(network->type())) {
      wifi_visible.push_back(std::move(*iter));
    } else {
      non_wifi_visible.push_back(std::move(*iter));
    }
  }

  // List active networks first (will always include Ethernet).
  network_list_ = std::move(active);

  // List non wifi visible networks next (Mobile and VPN).
  std::move(non_wifi_visible.begin(), non_wifi_visible.end(),
            std::back_inserter(network_list_));
  // List WiFi networks last.
  std::move(wifi_visible.begin(), wifi_visible.end(),
            std::back_inserter(network_list_));
  // Include hidden and new networks in the list at the end; they should not
  // be shown by the UI.
  std::move(hidden.begin(), hidden.end(), std::back_inserter(network_list_));
  std::move(new_networks.begin(), new_networks.end(),
            std::back_inserter(network_list_));
  network_list_sorted_ = true;
}

void NetworkStateHandler::DefaultNetworkServiceChanged(
    const std::string& service_path) {
  // Shill uses '/' for empty service path values; check explicitly for that.
  const char kEmptyServicePath[] = "/";
  std::string new_service_path =
      (service_path != kEmptyServicePath) ? service_path : "";
  if (new_service_path == default_network_path_) {
    return;
  }

  if (new_service_path.empty()) {
    // If Shill reports that there is no longer a default network but there is
    // still an active Tether connection corresponding to the default network,
    // return early without changing |default_network_path_|. Observers will be
    // notified of the default network change due to a subsequent call to
    // SetTetherNetworkStateDisconnected().
    const NetworkState* old_default_network = DefaultNetwork();
    if (old_default_network && old_default_network->type() == kTypeTether) {
      return;
    }
  }

  NET_LOG(EVENT) << "DefaultNetworkServiceChanged: "
                 << NetworkPathId(service_path);
  if (new_service_path.empty()) {
    // Notify that there is no default network.
    SetDefaultNetworkValues(/*path=*/std::string(), /*metered=*/false);
    NotifyDefaultNetworkChanged(kReasonChange);
    return;
  }

  const NetworkState* network = GetNetworkState(service_path);
  if (!network) {
    // If NetworkState is not available yet, do not notify observers here,
    // they will be notified when the state is received.
    NET_LOG(EVENT) << "Default NetworkState not available: "
                   << NetworkPathId(service_path);
    // Metered will be updated to the correct value when properties arrive.
    SetDefaultNetworkValues(service_path, /*metered=*/false);
    return;
  }

  if (!network->tether_guid().empty()) {
    DCHECK(network->type() == shill::kTypeWifi);

    // If the new default network from Shill's point of view is a Wi-Fi
    // network which corresponds to a hotspot for a Tether network, set the
    // default network to be the associated Tether network instead.
    network = GetNetworkStateFromGuid(network->tether_guid());
    if (default_network_path_ != network->path()) {
      NET_LOG(DEBUG) << "Tether network is default: " << NetworkId(network);
      SetDefaultNetworkValues(network->path(), /*metered=*/true);
      NotifyDefaultNetworkChanged(kReasonChange);
    }
    return;
  }

  // Request the updated default network properties which will trigger
  // NotifyDefaultNetworkChanged().
  // Metered will be updated to the correct value when properties arrive.
  SetDefaultNetworkValues(service_path, /*metered=*/false);
  RequestUpdateForNetwork(service_path);
}

//------------------------------------------------------------------------------
// Private methods

void NetworkStateHandler::UpdateGuid(NetworkState* network) {
  std::string specifier = network->GetSpecifier();
  DCHECK(!specifier.empty());
  if (!network->guid().empty()) {
    // If the network is saved in a profile, remove the entry from the map.
    // Otherwise ensure that the entry matches the specified GUID. (e.g. in
    // case a visible network with a specified guid gets configured with a
    // new guid). Exception: Ethernet expects to have a single network and a
    // consistent GUID.
    if (network->type() != shill::kTypeEthernet && network->IsInProfile()) {
      specifier_guid_map_.erase(specifier);
    } else {
      specifier_guid_map_[specifier] = network->guid();
    }
    return;
  }
  // Ensure that the NetworkState has a valid GUID.
  std::string guid;
  SpecifierGuidMap::iterator guid_iter = specifier_guid_map_.find(specifier);
  if (guid_iter != specifier_guid_map_.end()) {
    guid = guid_iter->second;
  } else {
    guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    specifier_guid_map_[specifier] = guid;
  }
  network->SetGuid(guid);
}

void NetworkStateHandler::HandleCellularNetworkUpdateReceived(
    NetworkState* network,
    bool had_icccid_before_update) {
  const DeviceState* device = GetDeviceState(network->device_path());
  if (!device) {
    return;
  }

  // One "roaming" state is shared between all cellular networks.
  network->provider_requires_roaming_ = device->provider_requires_roaming();

  const std::string& iccid = network->iccid();

  // If this network previously did not have an ICCID but just received one via
  // a property update, this may indicates that a stub cellular network has
  // transitioned to a Shill-backed network.
  if (!had_icccid_before_update && !iccid.empty() &&
      stub_cellular_networks_provider_) {
    std::string stub_service_path, stub_guid;
    bool replaced_stub =
        stub_cellular_networks_provider_->GetStubNetworkMetadata(
            iccid, device, &stub_service_path, &stub_guid);
    if (replaced_stub) {
      NotifyNetworkIdentifierTransitioned(stub_service_path, network->path(),
                                          stub_guid, network->guid());
    }
  }
}

bool NetworkStateHandler::AddOrRemoveStubCellularNetworks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!notifying_network_observers_);
  if (!stub_cellular_networks_provider_) {
    return false;
  }

  const DeviceState* device_state =
      GetDeviceStateByType(NetworkTypePattern::Cellular());
  ManagedStateList new_stub_networks;
  bool network_list_changed =
      stub_cellular_networks_provider_->AddOrRemoveStubCellularNetworks(
          network_list_, new_stub_networks, device_state);
  if (!new_stub_networks.size()) {
    return network_list_changed;
  }

  // Newly created stub cellular networks will not have a GUID. Assign GUIDs for
  // these new networks and add to network_list_.
  for (std::unique_ptr<ManagedState>& managed_state : new_stub_networks) {
    NetworkState* network = managed_state->AsNetworkState();
    UpdateGuid(network);
  }
  std::move(new_stub_networks.begin(), new_stub_networks.end(),
            std::back_inserter(network_list_));
  return true;
}

void NetworkStateHandler::NotifyNetworkListChanged() {
  NET_LOG(EVENT) << "NOTIFY: NetworkListChanged. Size: "
                 << network_list_.size();
  for (Observer& observer : observers_) {
    observer.NetworkListChanged();
  }
}

void NetworkStateHandler::NotifyDeviceListChanged() {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG(EVENT) << "NOTIFY: DeviceListChanged. Size: " << device_list_.size();
  for (Observer& observer : observers_) {
    observer.DeviceListChanged();
  }
}

DeviceState* NetworkStateHandler::GetModifiableDeviceState(
    const std::string& device_path) const {
  ManagedState* managed = GetModifiableManagedState(&device_list_, device_path);
  if (!managed) {
    return nullptr;
  }
  return managed->AsDeviceState();
}

DeviceState* NetworkStateHandler::GetModifiableDeviceStateByType(
    const NetworkTypePattern& type) const {
  for (const auto& device : device_list_) {
    if (TypeMatches(device.get(), type)) {
      return device->AsDeviceState();
    }
  }
  return nullptr;
}

NetworkState* NetworkStateHandler::GetModifiableNetworkState(
    const std::string& service_path) const {
  ManagedState* managed =
      GetModifiableManagedState(&network_list_, service_path);
  if (!managed) {
    managed = GetModifiableManagedState(&tether_network_list_, service_path);
    if (!managed) {
      return nullptr;
    }
  }
  return managed->AsNetworkState();
}

NetworkState* NetworkStateHandler::GetModifiableNetworkStateFromGuid(
    const std::string& guid) const {
  for (auto iter = tether_network_list_.begin();
       iter != tether_network_list_.end(); ++iter) {
    NetworkState* tether_network = (*iter)->AsNetworkState();
    if (tether_network->guid() == guid) {
      return tether_network;
    }
  }

  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    if (network->guid() == guid) {
      return network;
    }
  }

  return nullptr;
}

ManagedState* NetworkStateHandler::GetModifiableManagedState(
    const ManagedStateList* managed_list,
    const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto iter = managed_list->begin(); iter != managed_list->end(); ++iter) {
    ManagedState* managed = iter->get();
    if (managed->path() == path) {
      return managed;
    }
  }
  return nullptr;
}

NetworkStateHandler::ManagedStateList* NetworkStateHandler::GetManagedList(
    ManagedState::ManagedType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (type) {
    case ManagedState::MANAGED_TYPE_NETWORK:
      return &network_list_;
    case ManagedState::MANAGED_TYPE_DEVICE:
      return &device_list_;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void NetworkStateHandler::OnNetworkConnectionStateChanged(
    NetworkState* network) {
  DCHECK(network);
  bool default_changed = false;
  if (network->path() == default_network_path_) {
    default_changed = VerifyDefaultNetworkConnectionStateChange(network);
  }
  NotifyNetworkConnectionStateChanged(network);
  if (default_changed) {
    NotifyDefaultNetworkChanged(kReasonStateChange);
  }
}

bool NetworkStateHandler::VerifyDefaultNetworkConnectionStateChange(
    NetworkState* network) {
  DCHECK(network->path() == default_network_path_);
  if (network->IsConnectedState() ||
      NetworkState::StateIsPortalled(network->connection_state())) {
    return true;
  }
  if (network->IsConnectingState()) {
    // Wait until the network is actually connected to notify that the default
    // network changed.
    NET_LOG(DEBUG) << "Default network is connecting: " << NetworkId(network)
                   << "State: " << network->connection_state();
    return false;
  }
  NET_LOG(DEBUG) << "Default network not connected: " << NetworkId(network);
  return false;
}

void NetworkStateHandler::NotifyNetworkConnectionStateChanged(
    NetworkState* network) {
  DCHECK(network);
  SCOPED_NET_LOG_IF_SLOW();
  std::string desc = "NetworkConnectionStateChanged";
  if (network->path() == default_network_path_) {
    desc = "Default" + desc;
  }
  NET_LOG(EVENT) << "NOTIFY: " << desc << ": " << NetworkId(network) << ": "
                 << network->connection_state();
  notifying_network_observers_ = true;
  for (Observer& observer : observers_) {
    observer.NetworkConnectionStateChanged(network);
  }
  notifying_network_observers_ = false;
  NotifyIfActiveNetworksChanged();
}

void NetworkStateHandler::NotifyDefaultNetworkChanged(
    const std::string& log_reason) {
  SCOPED_NET_LOG_IF_SLOW();
  // If the default network is in an invalid state, |default_network_path_|
  // will be cleared; call DefaultNetworkChanged(nullptr).
  const NetworkState* default_network;
  if (default_network_path_.empty()) {
    default_network = nullptr;
  } else {
    default_network = GetModifiableNetworkState(default_network_path_);
    DCHECK(default_network) << "No default network: " << default_network_path_;
  }
  NET_LOG(EVENT) << "NOTIFY: DefaultNetworkChanged: "
                 << NetworkId(default_network) << ": " << log_reason;
  notifying_network_observers_ = true;
  for (Observer& observer : observers_) {
    observer.DefaultNetworkChanged(default_network);
  }
  notifying_network_observers_ = false;

  UpdatePortalStateAndNotify(default_network);
}

void NetworkStateHandler::UpdatePortalStateAndNotify(
    const NetworkState* default_network) {
  NetworkState::PortalState new_portal_state;
  std::string new_default_network_path;
  if (default_network &&
      (default_network->GetPortalState() != default_network_portal_state_ ||
       default_network->proxy_config() != default_network_proxy_config_)) {
    new_portal_state = default_network->GetPortalState();
    new_default_network_path = default_network->path();
    if (default_network->proxy_config()) {
      default_network_proxy_config_ = default_network->proxy_config()->Clone();
    } else {
      default_network_proxy_config_.reset();
    }
  } else if (!default_network && (default_network_portal_state_ !=
                                      NetworkState::PortalState::kUnknown ||
                                  default_network_proxy_config_.has_value())) {
    new_portal_state = NetworkState::PortalState::kUnknown;
    default_network_proxy_config_.reset();
  } else {
    // No portal state changes.
    return;
  }

  // Update metrics.
  if (new_default_network_path != default_network_path_) {
    // When the default network changes, update time histograms with a 0 result
    // to indicate a failure to transition to online.
    if (time_in_portal_) {
      SendPortalHistogramTimes(base::TimeDelta());
      time_in_portal_.reset();
    }
  } else {
    switch (new_portal_state) {
      case NetworkState::PortalState::kUnknown:
        // If we transition to an unknown state, update time histograms with a 0
        // result to indicate a failure to transition to online.
        if (time_in_portal_) {
          SendPortalHistogramTimes(base::TimeDelta());
          time_in_portal_.reset();
        }
        break;
      case NetworkState::PortalState::kOnline:
        if (time_in_portal_) {
          SendPortalHistogramTimes(time_in_portal_->Elapsed());
          time_in_portal_.reset();
        }
        break;
      case NetworkState::PortalState::kPortalSuspected:
        [[fallthrough]];
      case NetworkState::PortalState::kPortal:
        time_in_portal_ = base::ElapsedTimer();
        break;
      case NetworkState::PortalState::kNoInternet:
        // We don't track these states, reset the timer.
        time_in_portal_.reset();
        break;
    }
  }

  // Update the portal state after sending histograms.
  default_network_portal_state_ = new_portal_state;

  // Notify observers.
  NET_LOG(EVENT) << "NOTIFY: PortalStateChanged: "
                 << GetLogName(default_network) << ": "
                 << default_network_portal_state_;
  for (Observer& observer : observers_) {
    observer.PortalStateChanged(default_network, default_network_portal_state_);
  }
}

void NetworkStateHandler::SendPortalHistogramTimes(base::TimeDelta elapsed) {
  switch (default_network_portal_state_) {
    case NetworkState::PortalState::kPortal:
      base::UmaHistogramMediumTimes("Network.RedirectFoundToOnlineTime",
                                    elapsed);
      break;
    case NetworkState::PortalState::kPortalSuspected:
      base::UmaHistogramMediumTimes("Network.PortalSuspectedToOnlineTime",
                                    elapsed);
      break;
    default:
      // Previous state was not portalled, no times to report.
      break;
  }
}

bool NetworkStateHandler::ActiveNetworksChanged(
    const NetworkStateList& active_networks) {
  if (active_networks.size() != active_network_list_.size()) {
    return true;
  }
  for (size_t i = 0; i < active_network_list_.size(); ++i) {
    if (!active_network_list_[i].MatchesNetworkState(active_networks[i])) {
      return true;
    }
  }
  return false;
}

void NetworkStateHandler::NotifyIfActiveNetworksChanged() {
  SCOPED_NET_LOG_IF_SLOW();
  NetworkStateList active_networks;
  GetActiveNetworkListByType(NetworkTypePattern::Default(), &active_networks);
  if (!ActiveNetworksChanged(active_networks)) {
    return;
  }

  NET_LOG(EVENT) << "NOTIFY:ActiveNetworksChanged";

  active_network_list_.clear();
  active_network_list_.reserve(active_networks.size());
  for (const NetworkState* network : active_networks) {
    active_network_list_.emplace_back(network);
  }

  notifying_network_observers_ = true;
  for (Observer& observer : observers_) {
    observer.ActiveNetworksChanged(active_networks);
  }
  notifying_network_observers_ = false;
}

void NetworkStateHandler::NotifyNetworkPropertiesUpdated(
    const NetworkState* network) {
  // Skip property updates before NetworkState::InitialPropertiesReceived.
  if (network->type().empty()) {
    return;
  }
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG(EVENT) << "NOTIFY: NetworkPropertiesUpdated: " << NetworkId(network);
  notifying_network_observers_ = true;
  for (Observer& observer : observers_) {
    observer.NetworkPropertiesUpdated(network);
  }
  notifying_network_observers_ = false;
}

void NetworkStateHandler::NotifyDevicePropertiesUpdated(
    const DeviceState* device) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG(EVENT) << "NOTIFY: DevicePropertiesUpdated: " << device->path();
  for (Observer& observer : observers_) {
    observer.DevicePropertiesUpdated(device);
  }
}

void NetworkStateHandler::NotifyScanRequested(const NetworkTypePattern& type) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG(EVENT) << "NOTIFY: ScanRequested";
  for (Observer& observer : observers_) {
    observer.ScanRequested(type);
  }
}

void NetworkStateHandler::NotifyScanCompleted(const DeviceState* device) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG(EVENT) << "NOTIFY: ScanCompleted for: " << device->path();
  for (Observer& observer : observers_) {
    observer.ScanCompleted(device);
  }
}

void NetworkStateHandler::NotifyScanStarted(const DeviceState* device) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG(EVENT) << "NOTIFY: ScanStarted for: " << device->path();
  for (Observer& observer : observers_) {
    observer.ScanStarted(device);
  }
}

void NetworkStateHandler::NotifyNetworkIdentifierTransitioned(
    const std::string& old_service_path,
    const std::string& new_service_path,
    const std::string& old_guid,
    const std::string& new_guid) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG(EVENT) << "NOTIFY: NetworkIdentifierTransitioned: "
                 << "Service path: " << old_service_path << " => "
                 << new_service_path << ", GUID: " << old_guid << " => "
                 << new_guid;
  for (Observer& observer : observers_) {
    observer.NetworkIdentifierTransitioned(old_service_path, new_service_path,
                                           old_guid, new_guid);
  }
}

void NetworkStateHandler::LogPropertyUpdated(const ManagedState* state,
                                             const std::string& key,
                                             const base::Value& value) {
  std::string type_str =
      state->managed_type() == ManagedState::MANAGED_TYPE_DEVICE ? "Device"
      : state->path() == default_network_path_ ? "DefaultNetwork"
                                               : "Network";
  device_event_log::LogLevel log_level = device_event_log::LOG_LEVEL_EVENT;
  if (key == shill::kSignalStrengthProperty && !state->IsActive()) {
    log_level = device_event_log::LOG_LEVEL_DEBUG;
  }
  DEVICE_LOG(::device_event_log::LOG_TYPE_NETWORK, log_level)
      << type_str << "PropertyUpdated: " << GetLogName(state) << ", " << key
      << " = " << value;
}

std::string NetworkStateHandler::GetTechnologyForType(
    const NetworkTypePattern& type) const {
  if (type.MatchesType(shill::kTypeEthernet)) {
    return shill::kTypeEthernet;
  }

  if (type.MatchesType(shill::kTypeWifi)) {
    return shill::kTypeWifi;
  }

  if (type.MatchesType(shill::kTypeCellular)) {
    return shill::kTypeCellular;
  }

  if (type.MatchesType(kTypeTether)) {
    return kTypeTether;
  }

  NET_LOG(ERROR) << "Unexpected Type for technology: " << type.ToDebugString();
  return std::string();
}

std::vector<std::string> NetworkStateHandler::GetTechnologiesForType(
    const NetworkTypePattern& type) const {
  std::vector<std::string> technologies;
  if (type.MatchesType(shill::kTypeEthernet)) {
    technologies.emplace_back(shill::kTypeEthernet);
  }
  if (type.MatchesType(shill::kTypeWifi)) {
    technologies.emplace_back(shill::kTypeWifi);
  }
  if (type.MatchesType(shill::kTypeCellular)) {
    technologies.emplace_back(shill::kTypeCellular);
  }
  if (type.MatchesType(shill::kTypeVPN)) {
    technologies.emplace_back(shill::kTypeVPN);
  }
  if (type.MatchesType(kTypeTether)) {
    technologies.emplace_back(kTypeTether);
  }

  CHECK_GT(technologies.size(), 0ul);
  return technologies;
}

void NetworkStateHandler::SetDefaultNetworkValues(const std::string& path,
                                                  bool metered) {
  // If the default network changes, ensure that the portal state is updated.
  if (!path.empty()) {
    UpdatePortalStateAndNotify(GetNetworkState(path));
  }
  default_network_path_ = path;
  default_network_is_metered_ = metered;
}

void NetworkStateHandler::ProcessIsUserLoggedIn(
    const base::Value::List& profile_list) {
  // The profile list contains the shared profile on the login screen. Once the
  // user is logged in there is more than one profile in the profile list.
  is_user_logged_in_ = profile_list.size() > 1;
}

}  // namespace ash
