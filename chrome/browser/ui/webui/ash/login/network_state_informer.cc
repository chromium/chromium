// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/proxy_resolution/proxy_config.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kNetworkStateOffline[] = "offline";
const char kNetworkStateOnline[] = "online";
const char kNetworkStateCaptivePortal[] = "behind captive portal";
const char kNetworkStateConnecting[] = "connecting";
const char kNetworkStateProxyAuthRequired[] = "proxy auth required";
const char kNetworkStateUnknown[] = "unknown";

NetworkStateInformer::State GetStateForNetwork(const NetworkState* network) {
  if (!network) {
    return NetworkStateInformer::OFFLINE;
  }
  if (network->IsConnectingState()) {
    return NetworkStateInformer::CONNECTING;
  }
  if (!network->IsConnectedState()) {
    return NetworkStateInformer::OFFLINE;
  }
  switch (network->GetPortalState()) {
    case NetworkState::PortalState::kUnknown:
      return NetworkStateInformer::UNKNOWN;
    case NetworkState::PortalState::kOnline:
      return NetworkStateInformer::ONLINE;
    case NetworkState::PortalState::kPortalSuspected:
    case NetworkState::PortalState::kPortal:
      return NetworkStateInformer::CAPTIVE_PORTAL;
    case NetworkState::PortalState::kNoInternet:
      return NetworkStateInformer::OFFLINE;
  }
}

}  // namespace

NetworkStateInformer::NetworkStateInformer() : state_(OFFLINE) {}

NetworkStateInformer::~NetworkStateInformer() = default;

void NetworkStateInformer::Init() {
  UpdateState(NetworkHandler::Get()->network_state_handler()->DefaultNetwork());
  network_state_handler_observer_.Observe(
      NetworkHandler::Get()->network_state_handler());
}

void NetworkStateInformer::AddObserver(NetworkStateInformerObserver* observer) {
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void NetworkStateInformer::RemoveObserver(
    NetworkStateInformerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkStateInformer::DefaultNetworkChanged(const NetworkState* network) {
  UpdateStateAndNotify(network);
}

void NetworkStateInformer::PortalStateChanged(
    const NetworkState* network,
    const NetworkState::PortalState state) {
  UpdateStateAndNotify(network);
}

std::ostream& operator<<(std::ostream& stream,
                         const NetworkStateInformer::State& state) {
  switch (state) {
    case NetworkStateInformer::OFFLINE:
      stream << kNetworkStateOffline;
      break;
    case NetworkStateInformer::ONLINE:
      stream << kNetworkStateOnline;
      break;
    case NetworkStateInformer::CAPTIVE_PORTAL:
      stream << kNetworkStateCaptivePortal;
      break;
    case NetworkStateInformer::CONNECTING:
      stream << kNetworkStateConnecting;
      break;
    case NetworkStateInformer::PROXY_AUTH_REQUIRED:
      stream << kNetworkStateProxyAuthRequired;
      break;
    case NetworkStateInformer::UNKNOWN:
      stream << kNetworkStateUnknown;
      break;
  }
  return stream;
}

// static
// Returns network name by service path.
std::string NetworkStateInformer::GetNetworkName(
    const std::string& service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!network)
    return std::string();
  return network->name();
}

// static
bool NetworkStateInformer::IsProxyError(State state,
                                        NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::PROXY_AUTH_REQUIRED ||
         reason == NetworkError::ERROR_REASON_PROXY_AUTH_CANCELLED ||
         reason == NetworkError::ERROR_REASON_PROXY_CONNECTION_FAILED;
}

bool NetworkStateInformer::UpdateState(const NetworkState* network) {
  State new_state = GetStateForNetwork(network);
  std::string new_network_path;
  if (network)
    new_network_path = network->path();

  if (new_state == state_ && new_network_path == network_path_)
    return false;

  state_ = new_state;
  network_path_ = new_network_path;
  proxy_config_.reset();

  if (state_ == ONLINE) {
    for (NetworkStateInformerObserver& observer : observers_)
      observer.OnNetworkReady();
  }

  return true;
}

bool NetworkStateInformer::UpdateProxyConfig(const NetworkState* network) {
  if (!network) {
    return false;
  }

  if (proxy_config_ == network->proxy_config()) {
    return false;
  }

  if (network->proxy_config()) {
    proxy_config_ = network->proxy_config()->Clone();
  } else {
    proxy_config_.reset();
  }
  return true;
}

void NetworkStateInformer::UpdateStateAndNotify(const NetworkState* network) {
  bool state_changed = UpdateState(network);
  bool proxy_config_changed = UpdateProxyConfig(network);
  if (state_changed)
    SendStateToObservers(NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED);
  else if (proxy_config_changed)
    SendStateToObservers(NetworkError::ERROR_REASON_PROXY_CONFIG_CHANGED);
  else
    SendStateToObservers(NetworkError::ERROR_REASON_UPDATE);
}

void NetworkStateInformer::SendStateToObservers(
    NetworkError::ErrorReason reason) {
  for (NetworkStateInformerObserver& observer : observers_)
    observer.UpdateState(reason);
}

}  // namespace ash
