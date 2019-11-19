// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state_handler.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/tether_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Ignore changes to signal strength less than this value for active networks.
const int kSignalStrengthChangeThreshold = 5;

bool ConnectionStateChanged(const NetworkState* network,
                            const std::string& prev_connection_state,
                            bool prev_is_captive_portal) {
  if (network->is_captive_portal() != prev_is_captive_portal)
    return true;
  std::string connection_state = network->connection_state();
  bool prev_idle = prev_connection_state.empty() ||
                   prev_connection_state == shill::kStateIdle;
  bool cur_idle = connection_state == shill::kStateIdle;
  if (prev_idle || cur_idle)
    return prev_idle != cur_idle;
  return connection_state != prev_connection_state;
}

std::string GetManagedStateLogType(const ManagedState* state) {
  switch (state->managed_type()) {
    case ManagedState::MANAGED_TYPE_NETWORK:
      return "Network";
    case ManagedState::MANAGED_TYPE_DEVICE:
      return "Device";
  }
  NOTREACHED();
  return "";
}

std::string GetLogName(const ManagedState* state) {
  if (!state)
    return "None";
  return base::StringPrintf("%s (%s)", state->name().c_str(),
                            state->path().c_str());
}

bool ShouldIncludeNetworkInList(const NetworkState* network_state,
                                bool configured_only,
                                bool visible_only) {
  if (configured_only && !network_state->IsInProfile())
    return false;

  if (visible_only && !network_state->visible())
    return false;

  if (network_state->type() == shill::kTypeWifi &&
      !network_state->tether_guid().empty()) {
    // Wi-Fi networks which are actually underlying Wi-Fi hotspots for a
    // Tether network should not be included since they should only be shown
    // to the user as Tether networks.
    return false;
  }

  return true;
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
        signal_strength_(network->signal_strength()) {}

  bool MatchesNetworkState(const NetworkState* network) {
    return guid_ == network->guid() &&
           connection_state_ == network->connection_state() &&
           activation_state_ == network->activation_state() &&
           connect_requested_ == network->connect_requested() &&
           (abs(signal_strength_ - network->signal_strength()) <
            kSignalStrengthChangeThreshold);
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
  if (!did_shutdown_)
    Shutdown();
}

void NetworkStateHandler::Shutdown() {
  if (did_shutdown_)
    return;  // May get called twice in tests.
  did_shutdown_ = true;
  for (auto& observer : observers_)
    observer.OnShuttingDown();
}

void NetworkStateHandler::InitShillPropertyHandler() {
  shill_property_handler_ =
      std::make_unique<internal::ShillPropertyHandler>(this);
  shill_property_handler_->Init();
}

void NetworkStateHandler::UpdateBlockedWifiNetworks(
    bool only_managed,
    bool available_only,
    const std::vector<std::string>& blacklisted_hex_ssids) {
  if (allow_only_policy_networks_to_connect_ == only_managed &&
      allow_only_policy_networks_to_connect_if_available_ == available_only &&
      blacklisted_hex_ssids_ == blacklisted_hex_ssids) {
    return;
  }
  allow_only_policy_networks_to_connect_ = only_managed;
  allow_only_policy_networks_to_connect_if_available_ = available_only;
  blacklisted_hex_ssids_ = blacklisted_hex_ssids;

  UpdateBlockedWifiNetworksInternal();
}

const NetworkState* NetworkStateHandler::GetAvailableManagedWifiNetwork()
    const {
  DeviceState* device =
      GetModifiableDeviceStateByType(NetworkTypePattern::WiFi());
  const std::string& available_managed_network_path =
      device->available_managed_network_path();
  if (available_managed_network_path.empty())
    return nullptr;
  return GetNetworkState(available_managed_network_path);
}

bool NetworkStateHandler::OnlyManagedWifiNetworksAllowed() const {
  return allow_only_policy_networks_to_connect_ ||
         (allow_only_policy_networks_to_connect_if_available_ &&
          GetAvailableManagedWifiNetwork());
}

// static
std::unique_ptr<NetworkStateHandler> NetworkStateHandler::InitializeForTest() {
  auto handler = base::WrapUnique(new NetworkStateHandler());
  handler->InitShillPropertyHandler();
  return handler;
}

void NetworkStateHandler::AddObserver(NetworkStateHandlerObserver* observer,
                                      const base::Location& from_here) {
  observers_.AddObserver(observer);
  device_event_log::AddEntry(
      from_here.file_name(), from_here.line_number(),
      device_event_log::LOG_TYPE_NETWORK, device_event_log::LOG_LEVEL_DEBUG,
      base::StringPrintf("NetworkStateHandler::AddObserver: 0x%p", observer));
}

void NetworkStateHandler::RemoveObserver(NetworkStateHandlerObserver* observer,
                                         const base::Location& from_here) {
  observers_.RemoveObserver(observer);
  device_event_log::AddEntry(
      from_here.file_name(), from_here.line_number(),
      device_event_log::LOG_TYPE_NETWORK, device_event_log::LOG_LEVEL_DEBUG,
      base::StringPrintf("NetworkStateHandler::RemoveObserver: 0x%p",
                         observer));
}

bool NetworkStateHandler::HasObserver(NetworkStateHandlerObserver* observer) {
  return observers_.HasObserver(observer);
}

NetworkStateHandler::TechnologyState NetworkStateHandler::GetTechnologyState(
    const NetworkTypePattern& type) const {
  std::string technology = GetTechnologyForType(type);

  if (technology == kTypeTether) {
    return tether_technology_state_;
  }

  // If a technology is not in Shill's 'AvailableTechnologies' list, it is
  // always unavailable.
  if (!shill_property_handler_->IsTechnologyAvailable(technology))
    return TECHNOLOGY_UNAVAILABLE;

  // Prohibited should take precedence over other states.
  if (shill_property_handler_->IsTechnologyProhibited(technology))
    return TECHNOLOGY_PROHIBITED;

  // Disabling is a pseudostate used by the UI and takes precedence over
  // enabled.
  if (shill_property_handler_->IsTechnologyDisabling(technology)) {
    DCHECK(shill_property_handler_->IsTechnologyEnabled(technology));
    return TECHNOLOGY_DISABLING;
  }

  // Enabled and Uninitialized should be mutually exclusive. 'Enabling', which
  // is a pseudo state used by the UI, takes precedence over 'Uninitialized',
  // but not 'Enabled'.
  if (shill_property_handler_->IsTechnologyEnabled(technology))
    return TECHNOLOGY_ENABLED;
  if (shill_property_handler_->IsTechnologyEnabling(technology))
    return TECHNOLOGY_ENABLING;
  if (shill_property_handler_->IsTechnologyUninitialized(technology))
    return TECHNOLOGY_UNINITIALIZED;

  // Default state is 'Available', which is equivalent to 'Initialized but not
  // enabled'.
  return TECHNOLOGY_AVAILABLE;
}

void NetworkStateHandler::SetTechnologyEnabled(
    const NetworkTypePattern& type,
    bool enabled,
    const network_handler::ErrorCallback& error_callback) {
  std::vector<std::string> technologies = GetTechnologiesForType(type);
  for (const std::string& technology : technologies) {
    if (technology == kTypeTether) {
      if (tether_technology_state_ != TECHNOLOGY_ENABLED &&
          tether_technology_state_ != TECHNOLOGY_AVAILABLE) {
        NET_LOG(ERROR) << "SetTechnologyEnabled() called for the Tether "
                       << "DeviceState, but the current state was: "
                       << tether_technology_state_;
        network_handler::RunErrorCallback(
            error_callback, kTetherDevicePath,
            NetworkConnectionHandler::kErrorEnabledOrDisabledWhenNotAvailable,
            "");
        continue;
      }

      // Tether does not exist in Shill, so set |tether_technology_state_| and
      // skip the below interactions with |shill_property_handler_|.
      tether_technology_state_ =
          enabled ? TECHNOLOGY_ENABLED : TECHNOLOGY_AVAILABLE;
      continue;
    }

    if (!shill_property_handler_->IsTechnologyAvailable(technology))
      continue;
    NET_LOG_USER("SetTechnologyEnabled",
                 base::StringPrintf("%s:%d", technology.c_str(), enabled));
    shill_property_handler_->SetTechnologyEnabled(technology, enabled,
                                                  error_callback);
  }
  // Signal Device/Technology state changed.
  NotifyDeviceListChanged();
}

void NetworkStateHandler::SetTetherTechnologyState(
    TechnologyState technology_state) {
  if (tether_technology_state_ == technology_state)
    return;

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
  }
}

void NetworkStateHandler::SetProhibitedTechnologies(
    const std::vector<std::string>& prohibited_technologies,
    const network_handler::ErrorCallback& error_callback) {
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
      prohibited_technologies_copy, error_callback);
  // Signal Device/Technology state changed.
  NotifyDeviceListChanged();
}

const DeviceState* NetworkStateHandler::GetDeviceState(
    const std::string& device_path) const {
  const DeviceState* device = GetModifiableDeviceState(device_path);
  if (device && !device->update_received())
    return nullptr;
  return device;
}

const DeviceState* NetworkStateHandler::GetDeviceStateByType(
    const NetworkTypePattern& type) const {
  const DeviceState* device = GetModifiableDeviceStateByType(type);
  if (device && !device->update_received())
    return nullptr;
  return device;
}

bool NetworkStateHandler::GetScanningByType(
    const NetworkTypePattern& type) const {
  for (auto iter = device_list_.begin(); iter != device_list_.end(); ++iter) {
    const DeviceState* device = (*iter)->AsDeviceState();
    DCHECK(device);
    if (!device->update_received())
      continue;
    if (device->Matches(type) && device->scanning())
      return true;
  }
  return false;
}

const NetworkState* NetworkStateHandler::GetNetworkState(
    const std::string& service_path) const {
  const NetworkState* network = GetModifiableNetworkState(service_path);
  if (network && !network->update_received())
    return nullptr;
  return network;
}

const NetworkState* NetworkStateHandler::DefaultNetwork() const {
  if (default_network_path_.empty())
    return nullptr;
  return GetNetworkState(default_network_path_);
}

const NetworkState* NetworkStateHandler::ConnectedNetworkByType(
    const NetworkTypePattern& type) {
  NetworkStateList active_networks;
  GetActiveNetworkListByType(type, &active_networks);
  for (auto* network : active_networks) {
    if (network->IsConnectedState())
      return network;
  }
  return nullptr;
}

const NetworkState* NetworkStateHandler::ConnectingNetworkByType(
    const NetworkTypePattern& type) {
  NetworkStateList active_networks;
  GetActiveNetworkListByType(type, &active_networks);
  for (auto* network : active_networks) {
    if (network->IsConnectingState())
      return network;
  }
  return nullptr;
}

const NetworkState* NetworkStateHandler::ActiveNetworkByType(
    const NetworkTypePattern& type) {
  NetworkStateList active_networks;
  GetActiveNetworkListByType(type, &active_networks);
  if (active_networks.size() > 0)
    return active_networks.front();
  return nullptr;
}

const NetworkState* NetworkStateHandler::FirstNetworkByType(
    const NetworkTypePattern& type) {
  // Sort to ensure visible networks are listed first.
  if (!network_list_sorted_)
    SortNetworkList(false /* ensure_cellular */);

  const NetworkState* first_network = nullptr;
  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    if (!network->update_received())
      continue;
    if (!network->visible())
      break;
    if (network->Matches(type)) {
      first_network = network;
      break;
    }
  }

  // Active Ethernet networks are the highest priority.
  if (first_network && first_network->type() == shill::kTypeEthernet)
    return first_network;

  const NetworkState* first_tether_network =
      type.MatchesPattern(NetworkTypePattern::Tether()) &&
              !tether_network_list_.empty()
          ? tether_network_list_[0]->AsNetworkState()
          : nullptr;

  // Active Tether networks are next.
  if (first_tether_network && first_tether_network->IsConnectingOrConnected())
    return first_tether_network;

  // Other active networks are next.
  if (first_network && first_network->IsConnectingOrConnected())
    return first_network;

  // Non-active Tether networks are next.
  if (first_tether_network)
    return first_tether_network;

  // Other networks are last.
  return first_network;
}

void NetworkStateHandler::SetNetworkConnectRequested(
    const std::string& service_path,
    bool connect_requested) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (!network)
    return;
  network->connect_requested_ = connect_requested;
  network_list_sorted_ = false;
  OnNetworkConnectionStateChanged(network);
}

void NetworkStateHandler::SetNetworkChromePortalDetected(
    const std::string& service_path,
    bool portal_detected) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (!network || network->is_chrome_captive_portal_ == portal_detected)
    return;
  bool was_captive_portal = network->IsCaptivePortal();
  network->is_chrome_captive_portal_ = portal_detected;
  // Only notify a connection state change if IsCaptivePortal() changed, i.e.
  // is_chrome_captive_portal_ != (shill) is_captive_portal_.
  if (was_captive_portal == network->IsCaptivePortal())
    return;
  network_list_sorted_ = false;
  OnNetworkConnectionStateChanged(network);
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
  if (!device || device->mac_address().empty())
    return std::string();
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
  if (limit == 0)
    limit = std::numeric_limits<size_t>::max();

  if (!network_list_sorted_)
    SortNetworkList(false /* ensure_cellular */);

  // First, add active Tether networks.
  if (type.MatchesPattern(NetworkTypePattern::Tether()))
    AppendTetherNetworksToList(true /* get_active */, limit, list);

  // Second, add active non-Tether networks.
  for (const auto& managed : network_list_) {
    const NetworkState* network = managed.get()->AsNetworkState();
    DCHECK(network);
    if (!network->update_received() || !network->Matches(type))
      continue;
    if (!network->IsActive())
      break;  // Active networks are listed first.
    if (!ShouldIncludeNetworkInList(network, configured_only, visible_only))
      continue;

    if (network->type() == shill::kTypeEthernet) {
      // Ethernet networks should always be in front.
      list->insert(list->begin(), network);
    } else {
      list->push_back(network);
    }
    if (list->size() >= limit)
      return;
  }

  if (active_only)
    return;

  // Third, add inactive Tether networks.
  if (type.MatchesPattern(NetworkTypePattern::Tether()))
    AppendTetherNetworksToList(false /* get_active */, limit, list);
  if (list->size() >= limit)
    return;

  // Fourth, add inactive non-Tether networks.
  for (const auto& managed : network_list_) {
    const NetworkState* network = managed.get()->AsNetworkState();
    DCHECK(network);
    if (!network->update_received() || !network->Matches(type))
      continue;
    if (network->IsActive())
      continue;
    if (!ShouldIncludeNetworkInList(network, configured_only, visible_only))
      continue;
    list->push_back(network);
    if (list->size() >= limit)
      return;
  }
}

void NetworkStateHandler::AppendTetherNetworksToList(bool get_active,
                                                     size_t limit,
                                                     NetworkStateList* list) {
  DCHECK(list);
  DCHECK_NE(0U, limit);
  if (!IsTechnologyEnabled(NetworkTypePattern::Tether()))
    return;

  for (auto iter = tether_network_list_.begin();
       iter != tether_network_list_.end() && list->size() < limit; ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    if (network->IsConnectingOrConnected() != get_active)
      continue;
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
    if (!managed)
      return nullptr;
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
      NetworkState* tether_network = iter->get()->AsNetworkState();
      bool was_active = tether_network->IsConnectingOrConnected();
      NetworkState* wifi_network =
          GetModifiableNetworkStateFromGuid(tether_network->tether_guid());
      if (wifi_network)
        wifi_network->set_tether_guid(std::string());
      tether_network_list_.erase(iter);

      if (was_active)
        NotifyIfActiveNetworksChanged();
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
    NET_LOG(ERROR) << "Network is not a W-Fi network: " << wifi_network_guid;
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
    default_network_path_ = guid;
  }

  SetTetherNetworkStateConnectionState(guid, shill::kStateConfiguration);
}

void NetworkStateHandler::SetTetherNetworkStateConnected(
    const std::string& guid) {
  // Being connected implies that AssociateTetherNetworkStateWithWifiNetwork()
  // was already called, so ensure that the association is still intact.
  DCHECK(GetNetworkStateFromGuid(GetNetworkStateFromGuid(guid)->tether_guid())
             ->tether_guid() == guid);

  // At this point, there should be a default network set.
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

  DCHECK(!tether_network_state->is_captive_portal());
  if (ConnectionStateChanged(tether_network_state, prev_connection_state,
                             false /* prev_is_captive_portal */)) {
    NET_LOG(EVENT) << "Changing connection state for Tether network with GUID "
                   << guid << ". Old state: " << prev_connection_state << ", "
                   << "New state: " << connection_state;

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
      if (!should_be_present)
        device_list_.erase(it);
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
  if (network->type().empty() || !network->Matches(NetworkTypePattern::WiFi()))
    return false;

  bool prev_blocked_by_policy = network->blocked_by_policy();
  bool blocked_by_policy =
      !network->IsManagedByPolicy() &&
      (OnlyManagedWifiNetworksAllowed() ||
       base::Contains(blacklisted_hex_ssids_, network->GetHexSsid()));
  network->set_blocked_by_policy(blocked_by_policy);
  return prev_blocked_by_policy != blocked_by_policy;
}

void NetworkStateHandler::UpdateManagedWifiNetworkAvailable() {
  DeviceState* device =
      GetModifiableDeviceStateByType(NetworkTypePattern::WiFi());
  if (!device || !device->update_received())
    return;  // May be null in tests.

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
    UpdateBlockedWifiNetworksInternal();
    NotifyDevicePropertiesUpdated(device);
  }
}

void NetworkStateHandler::UpdateBlockedWifiNetworksInternal() {
  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    if (!network->Matches(NetworkTypePattern::WiFi()))
      continue;
    if (UpdateBlockedByPolicy(network))
      NotifyNetworkPropertiesUpdated(network);
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
    if (device->update_received() && device->Matches(type))
      list->push_back(device);
  }
}

void NetworkStateHandler::RequestScan(const NetworkTypePattern& type) {
  NET_LOG_USER("RequestScan", type.ToDebugString());
  if (type.MatchesPattern(NetworkTypePattern::WiFi())) {
    if (IsTechnologyEnabled(NetworkTypePattern::WiFi()))
      shill_property_handler_->RequestScanByType(shill::kTypeWifi);
    else if (type.Equals(NetworkTypePattern::WiFi()))
      return;  // Skip notify if disabled and wifi only requested.
  }
  if (type.Equals(NetworkTypePattern::Cellular()) ||
      type.Equals(NetworkTypePattern::Mobile())) {
    // Only request a Cellular scan if Cellular or Mobile is requested
    // explicitly.
    if (IsTechnologyEnabled(NetworkTypePattern::Cellular()))
      shill_property_handler_->RequestScanByType(shill::kTypeCellular);
    else if (type.Equals(NetworkTypePattern::Cellular()))
      return;  // Skip notify if disabled and cellular only requested.
  }

  // Note: for Tether we initiate the scan in the observer.
  NotifyScanRequested(type);
}

void NetworkStateHandler::RequestUpdateForNetwork(
    const std::string& service_path) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (network)
    network->set_update_requested(true);
  NET_LOG_EVENT("RequestUpdate", service_path);
  shill_property_handler_->RequestProperties(ManagedState::MANAGED_TYPE_NETWORK,
                                             service_path);
}

void NetworkStateHandler::SendUpdateNotificationForNetwork(
    const std::string& service_path) {
  const NetworkState* network = GetNetworkState(service_path);
  if (!network)
    return;
  NotifyNetworkPropertiesUpdated(network);
}

void NetworkStateHandler::ClearLastErrorForNetwork(
    const std::string& service_path) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (network)
    network->ClearError();
}

void NetworkStateHandler::SetCheckPortalList(
    const std::string& check_portal_list) {
  NET_LOG_EVENT("SetCheckPortalList", check_portal_list);
  shill_property_handler_->SetCheckPortalList(check_portal_list);
}

void NetworkStateHandler::SetCaptivePortalProviderForHexSsid(
    const std::string& hex_ssid,
    const std::string& provider_id,
    const std::string& provider_name) {
  NET_LOG(EVENT) << "SetCaptivePortalProviderForHexSsid: " << hex_ssid
                 << " -> (" << provider_id << ", " << provider_name << ")";
  // NetworkState hex SSIDs are always uppercase.
  std::string hex_ssid_uc = hex_ssid;
  transform(hex_ssid_uc.begin(), hex_ssid_uc.end(), hex_ssid_uc.begin(),
            toupper);
  if (provider_id.empty()) {
    hex_ssid_to_captive_portal_provider_map_.erase(hex_ssid_uc);
  } else {
    NetworkState::CaptivePortalProviderInfo provider_info;
    provider_info.id = provider_id;
    provider_info.name = provider_name;
    hex_ssid_to_captive_portal_provider_map_[hex_ssid_uc] =
        std::move(provider_info);
  }
  // When a new entry is added or removed from the map, check all networks
  // for a matching hex SSID and update the provider info. (This should occur
  // infrequently). New networks will be updated when added.
  for (auto& managed : network_list_) {
    NetworkState* network = managed->AsNetworkState();
    if (network->GetHexSsid() == hex_ssid_uc) {
      NET_LOG(EVENT) << "Setting captive portal provider for network: "
                     << network->guid() << " = " << provider_id;
      network->SetCaptivePortalProvider(provider_id, provider_name);
    }
  }
}

void NetworkStateHandler::SetWakeOnLanEnabled(bool enabled) {
  NET_LOG_EVENT("SetWakeOnLanEnabled", enabled ? "true" : "false");
  shill_property_handler_->SetWakeOnLanEnabled(enabled);
}

void NetworkStateHandler::SetHostname(const std::string& hostname) {
  NET_LOG_EVENT("SetHostname", hostname);
  shill_property_handler_->SetHostname(hostname);
}

void NetworkStateHandler::SetNetworkThrottlingStatus(
    bool enabled,
    uint32_t upload_rate_kbits,
    uint32_t download_rate_kbits) {
  NET_LOG_EVENT("SetNetworkThrottlingStatus",
                enabled ? ("true :" + base::NumberToString(upload_rate_kbits) +
                           ", " + base::NumberToString(download_rate_kbits))
                        : "false");
  shill_property_handler_->SetNetworkThrottlingStatus(
      enabled, upload_rate_kbits, download_rate_kbits);
}

void NetworkStateHandler::SetFastTransitionStatus(bool enabled) {
  NET_LOG_USER("SetFastTransitionStatus", enabled ? "true" : "false");
  shill_property_handler_->SetFastTransitionStatus(enabled);
}

const NetworkState* NetworkStateHandler::GetEAPForEthernet(
    const std::string& service_path,
    bool connected_only) {
  const NetworkState* network = GetNetworkState(service_path);
  if (!network) {
    NET_LOG(ERROR) << "GetEAPForEthernet: Unknown service: " << service_path;
    return nullptr;
  }
  if (network->type() != shill::kTypeEthernet) {
    NET_LOG(ERROR) << "GetEAPForEthernet: Not Ethernet: " << service_path;
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
                     << " for connected ethernet service: " << service_path;
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
          << service_path;
    }
    return nullptr;
  }
  return list.front();
}

void NetworkStateHandler::SetErrorForTest(const std::string& service_path,
                                          const std::string& error) {
  NetworkState* network_state = GetModifiableNetworkState(service_path);
  if (!network_state) {
    NET_LOG(ERROR) << "No matching NetworkState for: " << service_path;
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
                                            const base::ListValue& entries) {
  CHECK(!notifying_network_observers_);
  ManagedStateList* managed_list = GetManagedList(type);
  NET_LOG_DEBUG("UpdateManagedList: " + ManagedState::TypeToString(type),
                base::StringPrintf("%" PRIuS, entries.GetSize()));
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
  for (auto& iter : entries) {
    std::string path;
    iter.GetAsString(&path);
    if (path.empty() || path == shill::kFlimflamServicePath) {
      NET_LOG_ERROR(base::StringPrintf("Bad path in list:%d", type), path);
      continue;
    }
    auto found = managed_map.find(path);
    if (found == managed_map.end()) {
      if (list_entries.count(path) != 0) {
        NET_LOG_ERROR("Duplicate entry in list", path);
        continue;
      }
      managed_list->push_back(ManagedState::Create(type, path));
    } else {
      managed_list->push_back(std::move(found->second));
      managed_map.erase(found);
    }
    list_entries.insert(path);
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

  if (type != ManagedState::ManagedType::MANAGED_TYPE_NETWORK)
    return;

  // Remove associations Tether NetworkStates had with now removed Wi-Fi
  // NetworkStates.
  for (auto& iter : managed_map) {
    if (!iter.second->Matches(NetworkTypePattern::WiFi()))
      continue;

    NetworkState* tether_network = GetModifiableNetworkStateFromGuid(
        iter.second->AsNetworkState()->tether_guid());
    if (tether_network)
      tether_network->set_tether_guid(std::string());
  }
}

void NetworkStateHandler::ProfileListChanged() {
  NET_LOG_EVENT("ProfileListChanged", "Re-Requesting Network Properties");
  for (ManagedStateList::iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    shill_property_handler_->RequestProperties(
        ManagedState::MANAGED_TYPE_NETWORK, network->path());
  }
}

void NetworkStateHandler::UpdateManagedStateProperties(
    ManagedState::ManagedType type,
    const std::string& path,
    const base::Value& properties) {
  ManagedStateList* managed_list = GetManagedList(type);
  ManagedState* managed = GetModifiableManagedState(managed_list, path);
  if (!managed) {
    // The network has been removed from the list of networks.
    NET_LOG_DEBUG("UpdateManagedStateProperties: Not found", path);
    return;
  }
  managed->set_update_received();

  std::string desc = GetManagedStateLogType(managed) + " Properties Received";
  NET_LOG_DEBUG(desc, GetLogName(managed));

  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    UpdateNetworkStateProperties(managed->AsNetworkState(), properties);
  } else {
    // Device
    for (const auto iter : properties.DictItems())
      managed->PropertyChanged(iter.first, iter.second);
    managed->InitialPropertiesReceived(properties);
  }
  managed->set_update_requested(false);
}

void NetworkStateHandler::UpdateNetworkStateProperties(
    NetworkState* network,
    const base::Value& properties) {
  DCHECK(network);
  bool network_property_updated = false;
  std::string prev_connection_state = network->connection_state();
  bool prev_is_captive_portal = network->is_captive_portal();
  for (const auto iter : properties.DictItems()) {
    if (network->PropertyChanged(iter.first, iter.second))
      network_property_updated = true;
  }
  if (network->Matches(NetworkTypePattern::WiFi()))
    network_property_updated |= UpdateBlockedByPolicy(network);
  network_property_updated |= network->InitialPropertiesReceived(properties);

  UpdateGuid(network);
  UpdateCaptivePortalProvider(network);
  if (network->Matches(NetworkTypePattern::Cellular()))
    UpdateCellularStateFromDevice(network);

  network_list_sorted_ = false;

  // Notify observers of NetworkState changes.
  if (network_property_updated || network->update_requested()) {
    // Signal connection state changed after all properties have been updated.
    if (ConnectionStateChanged(network, prev_connection_state,
                               prev_is_captive_portal)) {
      OnNetworkConnectionStateChanged(network);
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
  if (!network || !network->update_received()) {
    // Shill may send a service property update before processing Chrome's
    // initial GetProperties request. If this occurs, the initial request will
    // include the changed property value so we can ignore this update.
    return;
  }
  std::string prev_connection_state = network->connection_state();
  bool prev_is_captive_portal = network->is_captive_portal();
  std::string prev_profile_path = network->profile_path();
  changed |= network->PropertyChanged(key, value);
  changed |= UpdateBlockedByPolicy(network);
  if (!changed)
    return;

  // If added to a Profile, request a full update so that a NetworkState
  // gets created.
  bool request_update =
      prev_profile_path.empty() && !network->profile_path().empty();
  bool sort_networks = false;
  bool notify_default = network->path() == default_network_path_;
  bool notify_connection_state = false;
  bool notify_active = false;

  if (key == shill::kStateProperty || key == shill::kVisibleProperty) {
    network_list_sorted_ = false;
    if (ConnectionStateChanged(network, prev_connection_state,
                               prev_is_captive_portal)) {
      notify_connection_state = true;
      notify_active = true;
      if (notify_default)
        notify_default = VerifyDefaultNetworkConnectionStateChange(network);
      // If the default network connection state changed, sort networks now
      // and ensure that a default cellular network exists.
      if (notify_default)
        sort_networks = true;

      // If the connection state changes, other properties such as IPConfig
      // may have changed, so request a full update.
      request_update = true;
    }
  } else if (key == shill::kActivationStateProperty) {
    // Activation state may affect "connecting" state in the UI.
    notify_connection_state = true;
    network_list_sorted_ = false;
  }

  if (request_update)
    RequestUpdateForNetwork(service_path);

  std::string value_str;
  value.GetAsString(&value_str);
  if (key == shill::kSignalStrengthProperty || key == shill::kWifiBSsid ||
      key == shill::kWifiFrequency ||
      key == shill::kWifiFrequencyListProperty ||
      (key == shill::kDeviceProperty && value_str == "/")) {
    // Uninteresting update. This includes 'Device' property changes to "/"
    // (occurs before just a service is removed).
    // For non active networks do not log or send any notifications.
    if (!network->IsActive())
      return;
    // Otherwise do not trigger 'default network changed'.
    notify_default = false;
    // Notify signal strength changes for active networks.
    if (key == shill::kSignalStrengthProperty)
      notify_active = true;
  }

  LogPropertyUpdated(network, key, value);
  if (notify_connection_state)
    NotifyNetworkConnectionStateChanged(network);
  if (notify_default)
    NotifyDefaultNetworkChanged();
  if (notify_active)
    NotifyIfActiveNetworksChanged();
  NotifyNetworkPropertiesUpdated(network);
  if (sort_networks)
    SortNetworkList(true /* ensure_cellular */);
}

void NetworkStateHandler::UpdateDeviceProperty(const std::string& device_path,
                                               const std::string& key,
                                               const base::Value& value) {
  SCOPED_NET_LOG_IF_SLOW();
  DeviceState* device = GetModifiableDeviceState(device_path);
  if (!device || !device->update_received()) {
    // Shill may send a device property update before processing Chrome's
    // initial GetProperties request. If this occurs, the initial request will
    // include the changed property value so we can ignore this update.
    return;
  }
  if (!device->PropertyChanged(key, value))
    return;

  LogPropertyUpdated(device, key, value);
  NotifyDevicePropertiesUpdated(device);

  if (key == shill::kScanningProperty && device->scanning() == false) {
    if (device->type() == shill::kTypeWifi)
      UpdateManagedWifiNetworkAvailable();
    NotifyScanCompleted(device);
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
}

void NetworkStateHandler::UpdateIPConfigProperties(
    ManagedState::ManagedType type,
    const std::string& path,
    const std::string& ip_config_path,
    const base::Value& properties) {
  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    NetworkState* network = GetModifiableNetworkState(path);
    if (!network)
      return;
    network->IPConfigPropertiesChanged(properties);
    NotifyNetworkPropertiesUpdated(network);
    if (network->path() == default_network_path_)
      NotifyDefaultNetworkChanged();
    if (network->IsActive())
      NotifyIfActiveNetworksChanged();
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    DeviceState* device = GetModifiableDeviceState(path);
    if (!device)
      return;
    device->IPConfigPropertiesChanged(ip_config_path, properties);
    NotifyDevicePropertiesUpdated(device);
    if (!default_network_path_.empty()) {
      const NetworkState* default_network =
          GetNetworkState(default_network_path_);
      if (default_network && default_network->device_path() == path) {
        NotifyNetworkPropertiesUpdated(default_network);
        NotifyDefaultNetworkChanged();
      }
    }
  }
}

void NetworkStateHandler::CheckPortalListChanged(
    const std::string& check_portal_list) {
  check_portal_list_ = check_portal_list;
}

void NetworkStateHandler::TechnologyListChanged() {
  // Eventually we would like to replace Technology state with Device state.
  // For now, treat technology state changes as device list changes.
  NotifyDeviceListChanged();
}

void NetworkStateHandler::ManagedStateListChanged(
    ManagedState::ManagedType type) {
  SCOPED_NET_LOG_IF_SLOW();
  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    SortNetworkList(true /* ensure_cellular */);
    UpdateNetworkStats();
    NotifyIfActiveNetworksChanged();
    NotifyNetworkListChanged();
    UpdateManagedWifiNetworkAvailable();
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    std::string devices;
    for (auto iter = device_list_.begin(); iter != device_list_.end(); ++iter) {
      if (iter != device_list_.begin())
        devices += ", ";
      devices += (*iter)->name();
    }
    NET_LOG_EVENT("DeviceList", devices);
    // A change to the device list may affect the default Cellular network, so
    // call SortNetworkList here.
    SortNetworkList(true /* ensure_cellular */);
    NotifyDeviceListChanged();
  } else {
    NOTREACHED();
  }
}

void NetworkStateHandler::SortNetworkList(bool ensure_cellular) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tether_sort_delegate_)
    tether_sort_delegate_->SortTetherNetworkList(&tether_network_list_);

  // Note: usually active networks will precede inactive networks, however
  // this may briefly be untrue during state transitions (e.g. a network may
  // transition to idle before the list is updated). Also separate inactive
  // Mobile and VPN networks (see below).
  ManagedStateList active, non_wifi_visible, wifi_visible, hidden, new_networks;
  int cellular_count = 0;
  bool have_default_cellular = false;
  for (ManagedStateList::iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    // NetworkState entries are created when they appear in the list, but the
    // details are not populated until an update is received.
    if (!network->update_received()) {
      new_networks.push_back(std::move(*iter));
      continue;
    }
    if (NetworkTypePattern::Cellular().MatchesType(network->type())) {
      ++cellular_count;
      if ((*iter)->AsNetworkState()->IsDefaultCellular())
        have_default_cellular = true;
    }
    if (network->IsActive()) {
      active.push_back(std::move(*iter));
      continue;
    }
    if (!network->visible()) {
      hidden.push_back(std::move(*iter));
      continue;
    }
    if (NetworkTypePattern::WiFi().MatchesType(network->type()))
      wifi_visible.push_back(std::move(*iter));
    else
      non_wifi_visible.push_back(std::move(*iter));
  }

  // List active networks first (will always include Ethernet).
  network_list_ = std::move(active);

  // If a default Cellular network is required, add it next.
  if (ensure_cellular && cellular_count == 0) {
    std::unique_ptr<NetworkState> default_cellular =
        MaybeCreateDefaultCellularNetwork();
    if (default_cellular)
      network_list_.push_back(std::move(default_cellular));
  }

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

  if (ensure_cellular && have_default_cellular) {
    // If we have created a default Cellular NetworkState, and we have > 1
    // Cellular NetworkState or no Cellular device, remove it.
    if (cellular_count > 1 ||
        !GetDeviceStateByType(NetworkTypePattern::Cellular())) {
      RemoveDefaultCellularNetwork();
    }
  }
}

void NetworkStateHandler::UpdateNetworkStats() {
  size_t shared = 0, unshared = 0, visible = 0;
  for (ManagedStateList::iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    if (network->visible())
      ++visible;
    if (network->IsInProfile()) {
      if (network->IsPrivate())
        ++unshared;
      else
        ++shared;
    }
  }
  UMA_HISTOGRAM_COUNTS_100("Networks.Visible", visible);
  UMA_HISTOGRAM_COUNTS_100("Networks.RememberedShared", shared);
  UMA_HISTOGRAM_COUNTS_100("Networks.RememberedUnshared", unshared);
}

void NetworkStateHandler::DefaultNetworkServiceChanged(
    const std::string& service_path) {
  // Shill uses '/' for empty service path values; check explicitly for that.
  const char kEmptyServicePath[] = "/";
  std::string new_service_path =
      (service_path != kEmptyServicePath) ? service_path : "";
  if (new_service_path == default_network_path_)
    return;

  if (new_service_path.empty()) {
    // If Shill reports that there is no longer a default network but there is
    // still an active Tether connection corresponding to the default network,
    // return early without changing |default_network_path_|. Observers will be
    // notified of the default network change due to a subsequent call to
    // SetTetherNetworkStateDisconnected().
    const NetworkState* old_default_network = DefaultNetwork();
    if (old_default_network && old_default_network->type() == kTypeTether)
      return;
  }

  default_network_path_ = service_path;
  NET_LOG_EVENT("DefaultNetworkServiceChanged:", default_network_path_);
  const NetworkState* network = nullptr;
  if (!default_network_path_.empty()) {
    network = GetNetworkState(default_network_path_);
    if (!network) {
      // If NetworkState is not available yet, do not notify observers here,
      // they will be notified when the state is received.
      NET_LOG(EVENT) << "Default NetworkState not available: "
                     << default_network_path_;
      return;
    }
    if (!network->tether_guid().empty()) {
      DCHECK(network->type() == shill::kTypeWifi);

      // If the new default network from Shill's point of view is a Wi-Fi
      // network which corresponds to a hotspot for a Tether network, set the
      // default network to be the associated Tether network instead.
      network = GetNetworkStateFromGuid(network->tether_guid());
      default_network_path_ = network->path();
    }
  }
  if (!network) {
    // Notify that there is no default network.
    NotifyDefaultNetworkChanged();
    return;
  }

  const std::string connection_state = network->connection_state();
  if (NetworkState::StateIsConnected(connection_state)) {
    NotifyDefaultNetworkChanged();
    return;
  }

  if (NetworkState::StateIsConnecting(connection_state)) {
    NET_LOG(EVENT) << "DefaultNetwork is connecting: " << GetLogName(network)
                   << ": " << connection_state;
  } else {
    NET_LOG(ERROR) << "DefaultNetwork in unexpected state: "
                   << GetLogName(network) << ": " << connection_state;
  }
  // Observers will be notified when the network becomes connected.
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
    // new guid). Exception: Ethernet and Cellular expect to have a single
    // network and a consistent GUID.
    if (network->type() != shill::kTypeEthernet &&
        network->type() != shill::kTypeCellular && network->IsInProfile()) {
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
    guid = base::GenerateGUID();
    specifier_guid_map_[specifier] = guid;
  }
  network->SetGuid(guid);
}

void NetworkStateHandler::UpdateCaptivePortalProvider(NetworkState* network) {
  auto portal_iter =
      hex_ssid_to_captive_portal_provider_map_.find(network->GetHexSsid());
  if (portal_iter == hex_ssid_to_captive_portal_provider_map_.end()) {
    network->SetCaptivePortalProvider("", "");
    return;
  }
  NET_LOG(EVENT) << "Setting captive portal provider for network: "
                 << network->guid() << " = " << portal_iter->second.id;
  network->SetCaptivePortalProvider(portal_iter->second.id,
                                    portal_iter->second.name);
}

void NetworkStateHandler::UpdateCellularStateFromDevice(NetworkState* network) {
  const DeviceState* device = GetDeviceState(network->device_path());
  if (!device)
    return;
  network->provider_requires_roaming_ = device->provider_requires_roaming();
}

std::unique_ptr<NetworkState>
NetworkStateHandler::MaybeCreateDefaultCellularNetwork() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!notifying_network_observers_);
  const DeviceState* device =
      GetDeviceStateByType(NetworkTypePattern::Cellular());
  // If no SIM is present there will not be useful user facing Device
  // information, so do not create a default Cellular network.
  if (!device || device->IsSimAbsent())
    return nullptr;
  // Create a default Cellular network. Properties from the associated Device
  // will be provided to the UI. Note that the network's name is left empty; UI
  // surfaces which attempt to show the network name will fall back to showing
  // the network type (i.e., "Cellular") instead.
  std::unique_ptr<NetworkState> network =
      NetworkState::CreateDefaultCellular(device->path());
  UpdateGuid(network.get());
  return network;
}

void NetworkStateHandler::RemoveDefaultCellularNetwork() {
  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    if ((*iter)->AsNetworkState()->IsDefaultCellular()) {
      network_list_.erase(iter);
      return;  // There will only ever be one default Cellular network.
    }
  }
}

void NetworkStateHandler::NotifyNetworkListChanged() {
  NET_LOG_EVENT("NOTIFY:NetworkListChanged",
                base::StringPrintf("Size:%" PRIuS, network_list_.size()));
  for (auto& observer : observers_)
    observer.NetworkListChanged();
}

void NetworkStateHandler::NotifyDeviceListChanged() {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG_DEBUG("NOTIFY:DeviceListChanged",
                base::StringPrintf("Size:%" PRIuS, device_list_.size()));
  for (auto& observer : observers_)
    observer.DeviceListChanged();
}

DeviceState* NetworkStateHandler::GetModifiableDeviceState(
    const std::string& device_path) const {
  ManagedState* managed = GetModifiableManagedState(&device_list_, device_path);
  if (!managed)
    return nullptr;
  return managed->AsDeviceState();
}

DeviceState* NetworkStateHandler::GetModifiableDeviceStateByType(
    const NetworkTypePattern& type) const {
  for (const auto& device : device_list_) {
    if (device->type().empty())
      continue;  // kTypeProperty not set yet, skip.
    if (device->Matches(type))
      return device->AsDeviceState();
  }
  return nullptr;
}

NetworkState* NetworkStateHandler::GetModifiableNetworkState(
    const std::string& service_path) const {
  ManagedState* managed =
      GetModifiableManagedState(&network_list_, service_path);
  if (!managed) {
    managed = GetModifiableManagedState(&tether_network_list_, service_path);
    if (!managed)
      return nullptr;
  }
  return managed->AsNetworkState();
}

NetworkState* NetworkStateHandler::GetModifiableNetworkStateFromGuid(
    const std::string& guid) const {
  for (auto iter = tether_network_list_.begin();
       iter != tether_network_list_.end(); ++iter) {
    NetworkState* tether_network = (*iter)->AsNetworkState();
    if (tether_network->guid() == guid)
      return tether_network;
  }

  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    if (network->guid() == guid)
      return network;
  }

  return nullptr;
}

ManagedState* NetworkStateHandler::GetModifiableManagedState(
    const ManagedStateList* managed_list,
    const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto iter = managed_list->begin(); iter != managed_list->end(); ++iter) {
    ManagedState* managed = iter->get();
    if (managed->path() == path)
      return managed;
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
  NOTREACHED();
  return nullptr;
}

void NetworkStateHandler::OnNetworkConnectionStateChanged(
    NetworkState* network) {
  DCHECK(network);
  bool default_changed = false;
  if (network->path() == default_network_path_)
    default_changed = VerifyDefaultNetworkConnectionStateChange(network);
  NotifyNetworkConnectionStateChanged(network);
  if (default_changed)
    NotifyDefaultNetworkChanged();
}

bool NetworkStateHandler::VerifyDefaultNetworkConnectionStateChange(
    NetworkState* network) {
  DCHECK(network->path() == default_network_path_);
  if (network->IsConnectedState())
    return true;
  if (network->IsConnectingState()) {
    // Wait until the network is actually connected to notify that the default
    // network changed.
    NET_LOG(EVENT) << "Default network is connecting: " << GetLogName(network)
                   << "State: " << network->connection_state();
    return false;
  }
  NET_LOG(ERROR) << "Default network in unexpected state: "
                 << GetLogName(network)
                 << "State: " << network->connection_state();
  default_network_path_.clear();
  return true;
}

void NetworkStateHandler::NotifyNetworkConnectionStateChanged(
    NetworkState* network) {
  DCHECK(network);
  SCOPED_NET_LOG_IF_SLOW();
  std::string desc = "NetworkConnectionStateChanged";
  if (network->path() == default_network_path_)
    desc = "Default" + desc;
  NET_LOG(EVENT) << "NOTIFY: " << desc << ": " << GetLogName(network) << ": "
                 << network->connection_state();
  notifying_network_observers_ = true;
  for (auto& observer : observers_)
    observer.NetworkConnectionStateChanged(network);
  notifying_network_observers_ = false;
  NotifyIfActiveNetworksChanged();
}

void NetworkStateHandler::NotifyDefaultNetworkChanged() {
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
  NET_LOG_EVENT("NOTIFY:DefaultNetworkChanged", GetLogName(default_network));
  notifying_network_observers_ = true;
  for (auto& observer : observers_)
    observer.DefaultNetworkChanged(default_network);
  notifying_network_observers_ = false;
}

bool NetworkStateHandler::ActiveNetworksChanged(
    const NetworkStateList& active_networks) {
  if (active_networks.size() != active_network_list_.size())
    return true;
  for (size_t i = 0; i < active_network_list_.size(); ++i) {
    if (!active_network_list_[i].MatchesNetworkState(active_networks[i]))
      return true;
  }
  return false;
}

void NetworkStateHandler::NotifyIfActiveNetworksChanged() {
  SCOPED_NET_LOG_IF_SLOW();
  NetworkStateList active_networks;
  GetActiveNetworkListByType(NetworkTypePattern::Default(), &active_networks);
  if (!ActiveNetworksChanged(active_networks))
    return;

  NET_LOG(EVENT) << "NOTIFY:ActiveNetworksChanged";

  active_network_list_.clear();
  active_network_list_.reserve(active_networks.size());
  for (const NetworkState* network : active_networks)
    active_network_list_.emplace_back(network);

  notifying_network_observers_ = true;
  for (auto& observer : observers_)
    observer.ActiveNetworksChanged(active_networks);
  notifying_network_observers_ = false;
}

void NetworkStateHandler::NotifyNetworkPropertiesUpdated(
    const NetworkState* network) {
  // Skip property updates before NetworkState::InitialPropertiesReceived.
  if (network->type().empty())
    return;
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG_EVENT("NOTIFY:NetworkPropertiesUpdated", GetLogName(network));
  notifying_network_observers_ = true;
  for (auto& observer : observers_)
    observer.NetworkPropertiesUpdated(network);
  notifying_network_observers_ = false;
}

void NetworkStateHandler::NotifyDevicePropertiesUpdated(
    const DeviceState* device) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG_EVENT("NOTIFY:DevicePropertiesUpdated", GetLogName(device));
  for (auto& observer : observers_)
    observer.DevicePropertiesUpdated(device);
}

void NetworkStateHandler::NotifyScanRequested(const NetworkTypePattern& type) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG_EVENT("NOTIFY:ScanRequested", "");
  for (auto& observer : observers_)
    observer.ScanRequested(type);
}

void NetworkStateHandler::NotifyScanCompleted(const DeviceState* device) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG_EVENT("NOTIFY:ScanCompleted", GetLogName(device));
  for (auto& observer : observers_)
    observer.ScanCompleted(device);
}

void NetworkStateHandler::LogPropertyUpdated(const ManagedState* state,
                                             const std::string& key,
                                             const base::Value& value) {
  std::string type_str =
      state->managed_type() == ManagedState::MANAGED_TYPE_DEVICE
          ? "Device"
          : state->path() == default_network_path_ ? "DefaultNetwork"
                                                   : "Network";
  device_event_log::LogLevel log_level =
      (key == shill::kErrorProperty || key == shill::kErrorDetailsProperty)
          ? device_event_log::LOG_LEVEL_ERROR
          : device_event_log::LOG_LEVEL_EVENT;
  DEVICE_LOG(::device_event_log::LOG_TYPE_NETWORK, log_level)
      << type_str << "PropertyUpdated: " << state->path() << " ("
      << state->name() << ") " << key << " = " << value;
}

std::string NetworkStateHandler::GetTechnologyForType(
    const NetworkTypePattern& type) const {
  if (type.MatchesType(shill::kTypeEthernet))
    return shill::kTypeEthernet;

  if (type.MatchesType(shill::kTypeWifi))
    return shill::kTypeWifi;

  if (type.MatchesType(shill::kTypeCellular))
    return shill::kTypeCellular;

  if (type.MatchesType(kTypeTether))
    return kTypeTether;

  NOTREACHED() << "Unexpected Type: " << type.ToDebugString();
  return std::string();
}

std::vector<std::string> NetworkStateHandler::GetTechnologiesForType(
    const NetworkTypePattern& type) const {
  std::vector<std::string> technologies;
  if (type.MatchesType(shill::kTypeEthernet))
    technologies.emplace_back(shill::kTypeEthernet);
  if (type.MatchesType(shill::kTypeWifi))
    technologies.emplace_back(shill::kTypeWifi);
  if (type.MatchesType(shill::kTypeCellular))
    technologies.emplace_back(shill::kTypeCellular);
  if (type.MatchesType(shill::kTypeBluetooth))
    technologies.emplace_back(shill::kTypeBluetooth);
  if (type.MatchesType(shill::kTypeVPN))
    technologies.emplace_back(shill::kTypeVPN);
  if (type.MatchesType(kTypeTether))
    technologies.emplace_back(kTypeTether);

  CHECK_GT(technologies.size(), 0ul);
  return technologies;
}

}  // namespace chromeos
