// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_connect.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_activation_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_profile.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

void IgnoreDisconnectError(const std::string& error_name) {}

const NetworkState* GetNetworkStateFromId(const std::string& network_id) {
  // Note: network_id === NetworkState::guid.
  return NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkStateFromGuid(network_id);
}

bool PreviousConnectAttemptHadError(const NetworkState* network) {
  const std::string& network_error = network->GetError();
  if (network_error.empty() || !network->IsSecure() ||
      network_error == shill::kErrorDisconnect) {
    return false;
  }
  NET_LOG(USER) << "Previous connect attempt for: " << NetworkId(network)
                << " had error: " << network_error;
  return true;
}

class NetworkConnectImpl : public NetworkConnect {
 public:
  explicit NetworkConnectImpl(Delegate* delegate);

  NetworkConnectImpl(const NetworkConnectImpl&) = delete;
  NetworkConnectImpl& operator=(const NetworkConnectImpl&) = delete;

  ~NetworkConnectImpl() override;

  // NetworkConnect
  void ConnectToNetworkId(const std::string& network_id) override;
  void DisconnectFromNetworkId(const std::string& network_id) override;
  void ShowMobileSetup(const std::string& network_id) override;
  void ShowCarrierAccountDetail(const std::string& network_id) override;
  void ShowCarrierUnlockNotification() override;
  void ShowPortalSignin(const std::string& network_id, Source source) override;
  void ConfigureNetworkIdAndConnect(const std::string& network_id,
                                    const base::Value::Dict& shill_properties,
                                    bool shared) override;
  void CreateConfigurationAndConnect(base::Value::Dict shill_properties,
                                     bool shared) override;
  void CreateConfiguration(base::Value::Dict shill_properties,
                           bool shared) override;

 private:
  void ActivateCellular(const std::string& network_id);
  void HandleUnconfiguredNetwork(const std::string& network_id);
  void OnConnectFailed(const std::string& network_id,
                       const std::string& error_name);
  bool GetNetworkProfilePath(bool shared, std::string* profile_path);
  void OnConnectSucceeded(const std::string& network_id);
  void CallConnectToNetwork(const std::string& network_id,
                            bool check_error_state);
  void OnConfigureFailed(const std::string& error_name);
  void OnConfigureSucceeded(bool connect_on_configure,
                            const std::string& service_path,
                            const std::string& network_id);
  void CallCreateConfiguration(base::Value::Dict properties,
                               bool shared,
                               bool connect_on_configure);
  void SetPropertiesFailed(const std::string& desc,
                           const std::string& network_id,
                           const std::string& config_error_name);
  void SetPropertiesToClear(base::Value::Dict* properties_to_set,
                            std::vector<std::string>* properties_to_clear);
  void ClearPropertiesAndConnect(
      const std::string& network_id,
      const std::vector<std::string>& properties_to_clear);
  void ConfigureSetProfileSucceeded(const std::string& network_id,
                                    base::Value::Dict properties_to_set);

  raw_ptr<Delegate> delegate_;
  base::WeakPtrFactory<NetworkConnectImpl> weak_factory_{this};
};

NetworkConnectImpl::NetworkConnectImpl(Delegate* delegate)
    : delegate_(delegate) {}

NetworkConnectImpl::~NetworkConnectImpl() = default;

void NetworkConnectImpl::HandleUnconfiguredNetwork(
    const std::string& network_id) {
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network) {
    NET_LOG(ERROR) << "Configuring unknown network: "
                   << NetworkGuidId(network_id);
    return;
  }

  if (network->type() == shill::kTypeWifi) {
    // If the network requires a password and is not the underlying Wi-Fi
    // hotspot for a Tether network, show the configure dialog.
    if (network->IsSecure() && network->tether_guid().empty())
      delegate_->ShowNetworkConfigure(network_id);
    return;
  }

  if (network->type() == shill::kTypeVPN) {
    // Third-party VPNs provide their own configuration UI.
    if (network->GetVpnProviderType() == shill::kProviderThirdPartyVpn)
      return;
    // Only fully configured policy VPNs are supported in the login screen.
    // See crbug.com/1167070#c53 for more info.
    if (!LoginState::Get()->IsUserLoggedIn())
      return;
    // Show the configure dialog for partially configured first-party VPNs.
    delegate_->ShowNetworkConfigure(network_id);
    return;
  }

  if (network->type() == shill::kTypeCellular) {
    if (network->RequiresActivation()) {
      ActivateCellular(network_id);
      return;
    }
    if (network->cellular_out_of_credits()) {
      ShowCarrierAccountDetail(network_id);
      return;
    }

    // If network is unconfigured because it's SIM locked, do nothing, as this
    // is handled by NetworkStateNotifier.
    if (network->GetError() == shill::kErrorSimLocked) {
      return;
    }
    if (network->GetError() == shill::kErrorSimCarrierLocked) {
      return;
    }

    // No special configure or setup for |network|, show the settings UI.
    if (LoginState::Get()->IsUserLoggedIn())
      delegate_->ShowNetworkSettings(network_id);
    return;
  }

  // If a tether network is unconfigured, do nothing, as this is handled by
  // TetherNotificationPresenter. A tether network is unconfigured when it
  // is connected via Bluetooth to the phone, but the phone has not yet
  // allowed mobile data to be used as a hotspot for the Chromebook. This
  // code path is run when the request to use the phone's mobile data
  // times out. We handle all tether request errors with
  // TetherNotificationPresenter, because there is no user action on the
  // Chromebook.
  if (network->type() == kTypeTether) {
    return;
  }

  DUMP_WILL_BE_NOTREACHED();
}

// If |shared| is true, sets |profile_path| to the shared profile path.
// Otherwise sets |profile_path| to the user profile path if authenticated and
// available. Returns 'false' if unable to set |profile_path|.
bool NetworkConnectImpl::GetNetworkProfilePath(bool shared,
                                               std::string* profile_path) {
  if (shared) {
    *profile_path = NetworkProfileHandler::GetSharedProfilePath();
    return true;
  }

  if (!LoginState::Get()->UserHasNetworkProfile()) {
    NET_LOG(ERROR) << "User profile specified before login";
    return false;
  }

  const NetworkProfile* profile =
      NetworkHandler::Get()->network_profile_handler()->GetDefaultUserProfile();
  if (!profile) {
    NET_LOG(ERROR) << "No user profile for unshared network configuration";
    return false;
  }

  *profile_path = profile->path;
  return true;
}

void NetworkConnectImpl::OnConnectFailed(const std::string& network_id,
                                         const std::string& error_name) {
  NET_LOG(ERROR) << "Connect Failed: " << error_name
                 << " For: " << NetworkGuidId(network_id);
  if (error_name == NetworkConnectionHandler::kErrorConnectFailed ||
      error_name == NetworkConnectionHandler::kErrorBadPassphrase ||
      error_name == NetworkConnectionHandler::kErrorPassphraseRequired ||
      error_name == NetworkConnectionHandler::kErrorConfigurationRequired ||
      error_name == NetworkConnectionHandler::kErrorAuthenticationRequired ||
      error_name == NetworkConnectionHandler::kErrorCellularOutOfCredits) {
    HandleUnconfiguredNetwork(network_id);
  } else if (error_name ==
             NetworkConnectionHandler::kErrorCertificateRequired) {
    // If ShowEnrollNetwork does fails, treat as an unconfigured network.
    if (!delegate_->ShowEnrollNetwork(network_id))
      HandleUnconfiguredNetwork(network_id);
  }
  // Notifications for other connect failures are handled by
  // NetworkStateNotifier, so no need to do anything else here.
}

void NetworkConnectImpl::OnConnectSucceeded(const std::string& network_id) {
  NET_LOG(USER) << "Connect Succeeded: " << NetworkGuidId(network_id);
}

// If |check_error_state| is true, error state for the network is checked,
// otherwise any current error state is ignored (e.g. for recently configured
// networks or repeat connect attempts).
void NetworkConnectImpl::CallConnectToNetwork(const std::string& network_id,
                                              bool check_error_state) {
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network) {
    OnConnectFailed(network_id, NetworkConnectionHandler::kErrorNotFound);
    return;
  }

  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      network->path(),
      base::BindOnce(&NetworkConnectImpl::OnConnectSucceeded,
                     weak_factory_.GetWeakPtr(), network_id),
      base::BindOnce(&NetworkConnectImpl::OnConnectFailed,
                     weak_factory_.GetWeakPtr(), network_id),
      check_error_state, ConnectCallbackMode::ON_COMPLETED);
}

void NetworkConnectImpl::OnConfigureFailed(const std::string& error_name) {
  NET_LOG(ERROR) << "Unable to configure network";
  delegate_->ShowNetworkConnectError(
      NetworkConnectionHandler::kErrorConfigureFailed, "");
}

void NetworkConnectImpl::OnConfigureSucceeded(bool connect_on_configure,
                                              const std::string& service_path,
                                              const std::string& network_id) {
  NET_LOG(USER) << "Configure Succeeded: " << NetworkGuidId(network_id);
  if (!connect_on_configure)
    return;
  // After configuring a network, ignore any (possibly stale) error state.
  const bool check_error_state = false;
  CallConnectToNetwork(network_id, check_error_state);
}

void NetworkConnectImpl::CallCreateConfiguration(
    base::Value::Dict shill_properties,
    bool shared,
    bool connect_on_configure) {
  std::string profile_path;
  if (!GetNetworkProfilePath(shared, &profile_path)) {
    delegate_->ShowNetworkConnectError(
        NetworkConnectionHandler::kErrorConfigureFailed, "");
    return;
  }
  shill_properties.Set(shill::kProfileProperty, profile_path);
  NetworkHandler::Get()
      ->network_configuration_handler()
      ->CreateShillConfiguration(
          std::move(shill_properties),
          base::BindOnce(&NetworkConnectImpl::OnConfigureSucceeded,
                         weak_factory_.GetWeakPtr(), connect_on_configure),
          base::BindOnce(&NetworkConnectImpl::OnConfigureFailed,
                         weak_factory_.GetWeakPtr()));
}

void NetworkConnectImpl::SetPropertiesFailed(
    const std::string& desc,
    const std::string& network_id,
    const std::string& config_error_name) {
  NET_LOG(ERROR) << desc << ": Failed: " << config_error_name
                 << "For: " << NetworkGuidId(network_id);
  delegate_->ShowNetworkConnectError(
      NetworkConnectionHandler::kErrorConfigureFailed, network_id);
}

void NetworkConnectImpl::SetPropertiesToClear(
    base::Value::Dict* properties_to_set,
    std::vector<std::string>* properties_to_clear) {
  // Move empty string properties to properties_to_clear.
  for (auto iter : *properties_to_set) {
    if (!iter.second.is_string()) {
      continue;
    }
    if (iter.second.GetString().empty()) {
      properties_to_clear->push_back(iter.first);
    }
  }
  // Remove cleared properties from properties_to_set.
  for (const std::string& property_to_clear : *properties_to_clear) {
    properties_to_set->Remove(property_to_clear);
  }
}

void NetworkConnectImpl::ClearPropertiesAndConnect(
    const std::string& network_id,
    const std::vector<std::string>& properties_to_clear) {
  NET_LOG(USER) << "ClearPropertiesAndConnect: " << NetworkGuidId(network_id);
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network) {
    SetPropertiesFailed("ClearProperties", network_id,
                        NetworkConnectionHandler::kErrorNotFound);
    return;
  }
  // After configuring a network, ignore any (possibly stale) error state.
  const bool check_error_state = false;
  NetworkHandler::Get()->network_configuration_handler()->ClearShillProperties(
      network->path(), properties_to_clear,
      base::BindOnce(&NetworkConnectImpl::CallConnectToNetwork,
                     weak_factory_.GetWeakPtr(), network_id, check_error_state),
      base::BindOnce(&NetworkConnectImpl::SetPropertiesFailed,
                     weak_factory_.GetWeakPtr(), "ClearProperties",
                     network_id));
}

void NetworkConnectImpl::ConfigureSetProfileSucceeded(
    const std::string& network_id,
    base::Value::Dict properties_to_set) {
  std::vector<std::string> properties_to_clear;
  SetPropertiesToClear(&properties_to_set, &properties_to_clear);
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network) {
    SetPropertiesFailed("SetProperties", network_id,
                        NetworkConnectionHandler::kErrorNotFound);
    return;
  }
  NetworkHandler::Get()->network_configuration_handler()->SetShillProperties(
      network->path(), properties_to_set,
      base::BindOnce(&NetworkConnectImpl::ClearPropertiesAndConnect,
                     weak_factory_.GetWeakPtr(), network_id,
                     properties_to_clear),
      base::BindOnce(&NetworkConnectImpl::SetPropertiesFailed,
                     weak_factory_.GetWeakPtr(), "SetProperties", network_id));
}

// Public methods

void NetworkConnectImpl::ConnectToNetworkId(const std::string& network_id) {
  NET_LOG(USER) << "ConnectToNetwork: " << NetworkGuidId(network_id);
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network) {
    OnConnectFailed(network_id, NetworkConnectionHandler::kErrorNotFound);
    return;
  }
  if (PreviousConnectAttemptHadError(network)) {
    // If the network is in an error state, show the configuration UI directly
    // to avoid a spurious notification.
    HandleUnconfiguredNetwork(network_id);
    return;
  }
  if (network->RequiresActivation()) {
    ActivateCellular(network_id);
    return;
  }
  if (network->type() == kTypeTether &&
      !network->tether_has_connected_to_host()) {
    delegate_->ShowNetworkConfigure(network_id);
    return;
  }

  CallConnectToNetwork(network_id, /*check_error_state=*/true);
}

void NetworkConnectImpl::DisconnectFromNetworkId(
    const std::string& network_id) {
  NET_LOG(USER) << "DisconnectFromNetwork: " << NetworkGuidId(network_id);
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network)
    return;
  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      network->path(), base::DoNothing(),
      base::BindOnce(&IgnoreDisconnectError));
}

void NetworkConnectImpl::ActivateCellular(const std::string& network_id) {
  NET_LOG(USER) << "ActivateCellular: " << NetworkGuidId(network_id);
  const NetworkState* cellular = GetNetworkStateFromId(network_id);
  if (!cellular || cellular->type() != shill::kTypeCellular) {
    NET_LOG(ERROR) << "ActivateCellular with no Service: "
                   << NetworkGuidId(network_id);
    return;
  }
  // Cellular activation now always goes through an online portal shown by the
  // mobile setup dialog.
  ShowMobileSetup(network_id);
}

void NetworkConnectImpl::ShowMobileSetup(const std::string& network_id) {
  const NetworkState* cellular = GetNetworkStateFromId(network_id);
  if (!cellular || cellular->type() != shill::kTypeCellular) {
    NET_LOG(ERROR) << "ShowMobileSetup without Cellular network: "
                   << NetworkGuidId(network_id);
    return;
  }
  if (cellular->activation_state() != shill::kActivationStateActivated &&
      cellular->activation_type() == shill::kActivationTypeNonCellular &&
      !NetworkHandler::Get()->network_state_handler()->DefaultNetwork()) {
    delegate_->ShowMobileActivationError(network_id);
    return;
  }
  delegate_->ShowMobileSetupDialog(network_id);
}

void NetworkConnectImpl::ShowCarrierAccountDetail(
    const std::string& network_id) {
  const NetworkState* cellular = GetNetworkStateFromId(network_id);
  if (!cellular || cellular->type() != shill::kTypeCellular) {
    NET_LOG(ERROR) << "ShowCarrierAccountDetail without Cellular network: "
                   << NetworkGuidId(network_id);
    return;
  }
  delegate_->ShowCarrierAccountDetail(network_id);
}

void NetworkConnectImpl::ShowCarrierUnlockNotification() {
  delegate_->ShowCarrierUnlockNotification();
}

void NetworkConnectImpl::ShowPortalSignin(const std::string& network_id,
                                          Source source) {
  delegate_->ShowPortalSignin(network_id, source);
}

void NetworkConnectImpl::ConfigureNetworkIdAndConnect(
    const std::string& network_id,
    const base::Value::Dict& properties,
    bool shared) {
  NET_LOG(USER) << "ConfigureNetworkIdAndConnect: "
                << NetworkGuidId(network_id);

  base::Value::Dict properties_to_set = properties.Clone();

  std::string profile_path;
  if (!GetNetworkProfilePath(shared, &profile_path)) {
    delegate_->ShowNetworkConnectError(
        NetworkConnectionHandler::kErrorConfigureFailed, network_id);
    return;
  }
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network) {
    delegate_->ShowNetworkConnectError(NetworkConnectionHandler::kErrorNotFound,
                                       network_id);
    return;
  }
  NetworkHandler::Get()->network_configuration_handler()->SetNetworkProfile(
      network->path(), profile_path,
      base::BindOnce(&NetworkConnectImpl::ConfigureSetProfileSucceeded,
                     weak_factory_.GetWeakPtr(), network_id,
                     std::move(properties_to_set)),
      base::BindOnce(&NetworkConnectImpl::SetPropertiesFailed,
                     weak_factory_.GetWeakPtr(), "SetProfile: " + profile_path,
                     network_id));
}

void NetworkConnectImpl::CreateConfigurationAndConnect(
    base::Value::Dict properties,
    bool shared) {
  NET_LOG(USER) << "CreateConfigurationAndConnect";
  CallCreateConfiguration(std::move(properties), shared,
                          true /* connect_on_configure */);
}

void NetworkConnectImpl::CreateConfiguration(base::Value::Dict properties,
                                             bool shared) {
  NET_LOG(USER) << "CreateConfiguration";
  CallCreateConfiguration(std::move(properties), shared,
                          false /* connect_on_configure */);
}

}  // namespace

static NetworkConnect* g_network_connect = NULL;

// static
void NetworkConnect::Initialize(Delegate* delegate) {
  CHECK(g_network_connect == NULL);
  g_network_connect = new NetworkConnectImpl(delegate);
}

// static
void NetworkConnect::Shutdown() {
  CHECK(g_network_connect);
  delete g_network_connect;
  g_network_connect = nullptr;
}

// static
bool NetworkConnect::IsInitialized() {
  return g_network_connect;
}

// static
NetworkConnect* NetworkConnect::Get() {
  CHECK(g_network_connect);
  return g_network_connect;
}

NetworkConnect::NetworkConnect() = default;

NetworkConnect::~NetworkConnect() = default;

}  // namespace ash
