// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_login_screen_protocol_handler_observer.h"

#include "base/metrics/histogram_macros.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

// A possible list of Wifi security types found on the login screen.
// It is important to note that the bigger the number, the lesser the security
// is. This will allow us to see how many managed users will be able to get
// online if their managed certificate secured network would be unavailable by
// using a PSK / enrolment / guest network instead.
// Note: Please do not add any new items here as this is a temporary experiment
// and has implications on the UMA metric!
enum class LowestNetworkSecurityType : uint8_t {
  kUnmanagedOpen = 0,
  kManagedOpen = 1,
  kUnmanagedPSK = 2,
  kManagedPSK = 3,
  kUnmanagedCert = 4,
  kManagedCert = 5,
  kUnknown = 6,  // This could happen in case the device is wired,
                 // or edge cases apply (e.g. there is only a
                 // spotty single WiFi network available).
  COUNT
};

// Check if the NetworkState is associated with a network which is available
// from the login screen.
bool IsDeviceNetwork(const NetworkState* network) {
  ::onc::ONCSource source = network->onc_source();
  return source == ::onc::ONC_SOURCE_DEVICE_POLICY || !network->IsPrivate();
}

}  // namespace

NetworkLoginScreenProtocolHandlerObserver::
    ~NetworkLoginScreenProtocolHandlerObserver() {
  if (network_state_handler_ && network_state_handler_->HasObserver(this)) {
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  }
}

void NetworkLoginScreenProtocolHandlerObserver::Init(
    NetworkStateHandler* network_state_handler) {
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);
}

void NetworkLoginScreenProtocolHandlerObserver::NetworkConnectionStateChanged(
    const NetworkState* network) {
  network_state_handler_->RequestScan(NetworkTypePattern::WiFi());
}

void NetworkLoginScreenProtocolHandlerObserver::ScanCompleted(
    const DeviceState* device) {
  if (device->type() != shill::kTypeWifi) {
    return;
  }

  // Collect the metrics for the networks of interest.
  NetworkStateHandler::NetworkStateList state_list;
  network_state_handler_->GetNetworkListByType(NetworkTypePattern::WiFi(),
                                               /*configured_only=*/true,
                                               /*visible_only=*/true,
                                               /*limit=*/0, &state_list);
  LowestNetworkSecurityType lowest_network_security_type =
      LowestNetworkSecurityType::kUnknown;
  for (const NetworkState* state : state_list) {
    if (state->blocked_by_policy() ||  // No forbidden networks.
        !state->connectable() ||       // Only networks we could connect to.
        !IsDeviceNetwork(state)) {     // Only device (aka login) networks.
      continue;
    }
    if (state->security_class() == shill::kSecurityClassWep ||
        state->security_class() == shill::kSecurityClassPsk) {
      lowest_network_security_type =
          std::min(lowest_network_security_type,
                   state->IsManagedByPolicy()
                       ? LowestNetworkSecurityType::kManagedPSK
                       : LowestNetworkSecurityType::kUnmanagedPSK);
      continue;
    }
    if (state->security_class() == shill::kSecurityClassNone) {
      lowest_network_security_type =
          std::min(lowest_network_security_type,
                   state->IsManagedByPolicy()
                       ? LowestNetworkSecurityType::kManagedOpen
                       : LowestNetworkSecurityType::kUnmanagedOpen);
      continue;
    }
    lowest_network_security_type = std::min(
        lowest_network_security_type,
        state->IsManagedByPolicy() ? LowestNetworkSecurityType::kManagedCert
                                   : LowestNetworkSecurityType::kUnmanagedCert);
  }
  UMA_HISTOGRAM_ENUMERATION(
      "Network.Ash.Wifi.WeakestLoginConnectionAvailability",
      lowest_network_security_type, LowestNetworkSecurityType::COUNT);
  // Make sure we only do this once
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

}  // namespace ash
