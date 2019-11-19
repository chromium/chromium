// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/proxy_resolution/proxy_config.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kNetworkStateOffline[] = "offline";
const char kNetworkStateOnline[] = "online";
const char kNetworkStateCaptivePortal[] = "behind captive portal";
const char kNetworkStateConnecting[] = "connecting";
const char kNetworkStateProxyAuthRequired[] = "proxy auth required";

NetworkStateInformer::State GetStateForDefaultNetwork() {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!network)
    return NetworkStateInformer::OFFLINE;

  if (network_portal_detector::GetInstance()->IsEnabled()) {
    NetworkPortalDetector::CaptivePortalState state =
        network_portal_detector::GetInstance()->GetCaptivePortalState(
            network->guid());
    NetworkPortalDetector::CaptivePortalStatus status = state.status;
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN &&
        NetworkState::StateIsConnecting(network->connection_state())) {
      return NetworkStateInformer::CONNECTING;
    }
    // For proxy-less networks rely on shill's online state if
    // NetworkPortalDetector's state of current network is unknown.
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE ||
        (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN &&
         !NetworkHandler::Get()
              ->ui_proxy_config_service()
              ->HasDefaultNetworkProxyConfigured() &&
         network->connection_state() == shill::kStateOnline)) {
      return NetworkStateInformer::ONLINE;
    }
    if (status ==
            NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED &&
        NetworkHandler::Get()
            ->ui_proxy_config_service()
            ->HasDefaultNetworkProxyConfigured()) {
      return NetworkStateInformer::PROXY_AUTH_REQUIRED;
    }
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL ||
        (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN &&
         network->is_captive_portal()))
      return NetworkStateInformer::CAPTIVE_PORTAL;
  } else {
    if (NetworkState::StateIsConnecting(network->connection_state()))
      return NetworkStateInformer::CONNECTING;
    if (network->connection_state() == shill::kStateOnline)
      return NetworkStateInformer::ONLINE;
    if (network->is_captive_portal())
      return NetworkStateInformer::CAPTIVE_PORTAL;
  }

  // If there is no connection to the internet report it as online for the
  // Active Directory devices. These devices does not have to be online to reach
  // the server.
  // TODO(rsorokin): Fix reporting network connectivity for Active Directory
  // devices. (see crbug.com/685691)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsActiveDirectoryManaged())
    return NetworkStateInformer::ONLINE;

  return NetworkStateInformer::OFFLINE;
}

}  // namespace

NetworkStateInformer::NetworkStateInformer() : state_(OFFLINE) {}

NetworkStateInformer::~NetworkStateInformer() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
  network_portal_detector::GetInstance()->RemoveObserver(this);
}

void NetworkStateInformer::Init() {
  UpdateState();
  NetworkHandler::Get()->network_state_handler()->AddObserver(
      this, FROM_HERE);

  network_portal_detector::GetInstance()->AddAndFireObserver(this);
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
  UpdateStateAndNotify();
}

void NetworkStateInformer::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  UpdateStateAndNotify();
}

void NetworkStateInformer::OnPortalDetected() {
  UpdateStateAndNotify();
}

// static
const char* NetworkStateInformer::StatusString(State state) {
  switch (state) {
    case OFFLINE:
      return kNetworkStateOffline;
    case ONLINE:
      return kNetworkStateOnline;
    case CAPTIVE_PORTAL:
      return kNetworkStateCaptivePortal;
    case CONNECTING:
      return kNetworkStateConnecting;
    case PROXY_AUTH_REQUIRED:
      return kNetworkStateProxyAuthRequired;
    default:
      NOTREACHED();
      return NULL;
  }
}

bool NetworkStateInformer::UpdateState() {
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  State new_state = GetStateForDefaultNetwork();
  std::string new_network_path;
  if (default_network)
    new_network_path = default_network->path();

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

bool NetworkStateInformer::UpdateProxyConfig() {
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network || !default_network->proxy_config())
    return false;

  if (proxy_config_ && *proxy_config_ == *default_network->proxy_config())
    return false;
  proxy_config_ =
      std::make_unique<base::Value>(default_network->proxy_config()->Clone());
  return true;
}

void NetworkStateInformer::UpdateStateAndNotify() {
  bool state_changed = UpdateState();
  bool proxy_config_changed = UpdateProxyConfig();
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

}  // namespace chromeos
