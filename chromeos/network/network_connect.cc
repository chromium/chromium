// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_connect.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_activation_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/tether_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void IgnoreDisconnectError(const std::string& error_name,
                           std::unique_ptr<base::DictionaryValue> error_data) {}

const NetworkState* GetNetworkStateFromId(const std::string& network_id) {
  // Note: network_id === NetworkState::guid.
  return NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkStateFromGuid(network_id);
}

class NetworkConnectImpl : public NetworkConnect {
 public:
  explicit NetworkConnectImpl(Delegate* delegate);
  ~NetworkConnectImpl() override;

  // NetworkConnect
  void ConnectToNetworkId(const std::string& network_id) override;
  void DisconnectFromNetworkId(const std::string& network_id) override;
  void SetTechnologyEnabled(const NetworkTypePattern& technology,
                            bool enabled_state) override;
  void ShowMobileSetup(const std::string& network_id) override;
  void ConfigureNetworkIdAndConnect(
      const std::string& network_id,
      const base::DictionaryValue& shill_properties,
      bool shared) override;
  void CreateConfigurationAndConnect(base::DictionaryValue* shill_properties,
                                     bool shared) override;
  void CreateConfiguration(base::DictionaryValue* shill_properties,
                           bool shared) override;

 private:
  void ActivateCellular(const std::string& network_id);
  void HandleUnconfiguredNetwork(const std::string& network_id);
  void OnConnectFailed(const std::string& network_id,
                       const std::string& error_name,
                       std::unique_ptr<base::DictionaryValue> error_data);
  bool GetNetworkProfilePath(bool shared, std::string* profile_path);
  void OnConnectSucceeded(const std::string& network_id);
  void CallConnectToNetwork(const std::string& network_id,
                            bool check_error_state);
  void OnConfigureFailed(const std::string& error_name,
                         std::unique_ptr<base::DictionaryValue> error_data);
  void OnConfigureSucceeded(bool connect_on_configure,
                            const std::string& service_path,
                            const std::string& network_id);
  void CallCreateConfiguration(base::DictionaryValue* properties,
                               bool shared,
                               bool connect_on_configure);
  void SetPropertiesFailed(const std::string& desc,
                           const std::string& network_id,
                           const std::string& config_error_name,
                           std::unique_ptr<base::DictionaryValue> error_data);
  void SetPropertiesToClear(base::DictionaryValue* properties_to_set,
                            std::vector<std::string>* properties_to_clear);
  void ClearPropertiesAndConnect(
      const std::string& network_id,
      const std::vector<std::string>& properties_to_clear);
  void ConfigureSetProfileSucceeded(
      const std::string& network_id,
      std::unique_ptr<base::DictionaryValue> properties_to_set);

  Delegate* delegate_;
  base::WeakPtrFactory<NetworkConnectImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectImpl);
};

NetworkConnectImpl::NetworkConnectImpl(Delegate* delegate)
    : delegate_(delegate) {}

NetworkConnectImpl::~NetworkConnectImpl() = default;

void NetworkConnectImpl::HandleUnconfiguredNetwork(
    const std::string& network_id) {
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network) {
    NET_LOG_ERROR("Configuring unknown network", network_id);
    return;
  }

  if (network->type() == shill::kTypeWifi) {
    // If the network does not require a password, do not show the dialog since
    // there is nothing to configure. Likewise, if the network is the underlying
    // Wi-Fi hotspot for a Tether network, do not show the dialog since the
    // Tether component handles this case itself.
    if (network->security_class() != shill::kSecurityNone &&
        network->tether_guid().empty()) {
      delegate_->ShowNetworkConfigure(network_id);
    }
    return;
  }

  if (network->type() == shill::kTypeVPN) {
    // Third-party VPNs handle configuration UI themselves.
    if (network->GetVpnProviderType() != shill::kProviderThirdPartyVpn)
      delegate_->ShowNetworkConfigure(network_id);
    return;
  }

  if (network->type() == shill::kTypeCellular) {
    if (network->RequiresActivation()) {
      ActivateCellular(network_id);
      return;
    }
    if (network->cellular_out_of_credits()) {
      ShowMobileSetup(network_id);
      return;
    }
    // No special configure or setup for |network|, show the settings UI.
    if (LoginState::Get()->IsUserLoggedIn())
      delegate_->ShowNetworkSettings(network_id);
    return;
  }
  NOTREACHED();
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
    NET_LOG_ERROR("User profile specified before login", "");
    return false;
  }

  const NetworkProfile* profile =
      NetworkHandler::Get()->network_profile_handler()->GetDefaultUserProfile();
  if (!profile) {
    NET_LOG_ERROR("No user profile for unshared network configuration", "");
    return false;
  }

  *profile_path = profile->path;
  return true;
}

void NetworkConnectImpl::OnConnectFailed(
    const std::string& network_id,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG(ERROR) << "Connect Failed: " << error_name << " For: " << network_id;

  if (error_name == NetworkConnectionHandler::kErrorConnectFailed ||
      error_name == NetworkConnectionHandler::kErrorBadPassphrase ||
      error_name == NetworkConnectionHandler::kErrorPassphraseRequired ||
      error_name == NetworkConnectionHandler::kErrorConfigurationRequired ||
      error_name == NetworkConnectionHandler::kErrorAuthenticationRequired) {
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
  NET_LOG_USER("Connect Succeeded", network_id);
}

// If |check_error_state| is true, error state for the network is checked,
// otherwise any current error state is ignored (e.g. for recently configured
// networks or repeat connect attempts).
void NetworkConnectImpl::CallConnectToNetwork(const std::string& network_id,
                                              bool check_error_state) {
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network) {
    OnConnectFailed(network_id, NetworkConnectionHandler::kErrorNotFound,
                    nullptr);
    return;
  }

  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      network->path(),
      base::Bind(&NetworkConnectImpl::OnConnectSucceeded,
                 weak_factory_.GetWeakPtr(), network_id),
      base::Bind(&NetworkConnectImpl::OnConnectFailed,
                 weak_factory_.GetWeakPtr(), network_id),
      check_error_state, ConnectCallbackMode::ON_COMPLETED);
}

void NetworkConnectImpl::OnConfigureFailed(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG_ERROR("Unable to configure network", "");
  delegate_->ShowNetworkConnectError(
      NetworkConnectionHandler::kErrorConfigureFailed, "");
}

void NetworkConnectImpl::OnConfigureSucceeded(bool connect_on_configure,
                                              const std::string& service_path,
                                              const std::string& network_id) {
  NET_LOG_USER("Configure Succeeded", network_id);
  if (!connect_on_configure)
    return;
  // After configuring a network, ignore any (possibly stale) error state.
  const bool check_error_state = false;
  CallConnectToNetwork(network_id, check_error_state);
}

void NetworkConnectImpl::CallCreateConfiguration(
    base::DictionaryValue* shill_properties,
    bool shared,
    bool connect_on_configure) {
  std::string profile_path;
  if (!GetNetworkProfilePath(shared, &profile_path)) {
    delegate_->ShowNetworkConnectError(
        NetworkConnectionHandler::kErrorConfigureFailed, "");
    return;
  }
  shill_properties->SetKey(shill::kProfileProperty, base::Value(profile_path));
  NetworkHandler::Get()
      ->network_configuration_handler()
      ->CreateShillConfiguration(
          *shill_properties,
          base::Bind(&NetworkConnectImpl::OnConfigureSucceeded,
                     weak_factory_.GetWeakPtr(), connect_on_configure),
          base::Bind(&NetworkConnectImpl::OnConfigureFailed,
                     weak_factory_.GetWeakPtr()));
}

void NetworkConnectImpl::SetPropertiesFailed(
    const std::string& desc,
    const std::string& network_id,
    const std::string& config_error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG_ERROR(desc + ": Failed: " + config_error_name, network_id);
  delegate_->ShowNetworkConnectError(
      NetworkConnectionHandler::kErrorConfigureFailed, network_id);
}

void NetworkConnectImpl::SetPropertiesToClear(
    base::DictionaryValue* properties_to_set,
    std::vector<std::string>* properties_to_clear) {
  // Move empty string properties to properties_to_clear.
  for (base::DictionaryValue::Iterator iter(*properties_to_set);
       !iter.IsAtEnd(); iter.Advance()) {
    std::string value_str;
    if (iter.value().GetAsString(&value_str) && value_str.empty())
      properties_to_clear->push_back(iter.key());
  }
  // Remove cleared properties from properties_to_set.
  for (std::vector<std::string>::iterator iter = properties_to_clear->begin();
       iter != properties_to_clear->end(); ++iter) {
    properties_to_set->RemoveWithoutPathExpansion(*iter, NULL);
  }
}

void NetworkConnectImpl::ClearPropertiesAndConnect(
    const std::string& network_id,
    const std::vector<std::string>& properties_to_clear) {
  NET_LOG_USER("ClearPropertiesAndConnect", network_id);
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network) {
    SetPropertiesFailed("ClearProperties", network_id,
                        NetworkConnectionHandler::kErrorNotFound, nullptr);
    return;
  }
  // After configuring a network, ignore any (possibly stale) error state.
  const bool check_error_state = false;
  NetworkHandler::Get()->network_configuration_handler()->ClearShillProperties(
      network->path(), properties_to_clear,
      base::Bind(&NetworkConnectImpl::CallConnectToNetwork,
                 weak_factory_.GetWeakPtr(), network_id, check_error_state),
      base::Bind(&NetworkConnectImpl::SetPropertiesFailed,
                 weak_factory_.GetWeakPtr(), "ClearProperties", network_id));
}

void NetworkConnectImpl::ConfigureSetProfileSucceeded(
    const std::string& network_id,
    std::unique_ptr<base::DictionaryValue> properties_to_set) {
  std::vector<std::string> properties_to_clear;
  SetPropertiesToClear(properties_to_set.get(), &properties_to_clear);
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network) {
    SetPropertiesFailed("SetProperties", network_id,
                        NetworkConnectionHandler::kErrorNotFound, nullptr);
    return;
  }
  NetworkHandler::Get()->network_configuration_handler()->SetShillProperties(
      network->path(), *properties_to_set,
      base::Bind(&NetworkConnectImpl::ClearPropertiesAndConnect,
                 weak_factory_.GetWeakPtr(), network_id, properties_to_clear),
      base::Bind(&NetworkConnectImpl::SetPropertiesFailed,
                 weak_factory_.GetWeakPtr(), "SetProperties", network_id));
}

// Public methods

void NetworkConnectImpl::ConnectToNetworkId(const std::string& network_id) {
  NET_LOG_USER("ConnectToNetwork", network_id);
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (network) {
    const std::string& network_error = network->GetError();
    if (!network_error.empty() && !network->security_class().empty()) {
      NET_LOG_USER("Configure: " + network_error, network_id);
      // If the network is in an error state, show the configuration UI
      // directly to avoid a spurious notification.
      HandleUnconfiguredNetwork(network_id);
      return;
    } else if (network->RequiresActivation()) {
      ActivateCellular(network_id);
      return;
    } else if (network->type() == kTypeTether &&
               !network->tether_has_connected_to_host()) {
      delegate_->ShowNetworkConfigure(network_id);
      return;
    }
  }
  const bool check_error_state = true;
  CallConnectToNetwork(network_id, check_error_state);
}

void NetworkConnectImpl::DisconnectFromNetworkId(
    const std::string& network_id) {
  NET_LOG_USER("DisconnectFromNetwork", network_id);
  const NetworkState* network = GetNetworkStateFromId(network_id);
  if (!network)
    return;
  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      network->path(), base::DoNothing(), base::Bind(&IgnoreDisconnectError));
}

void NetworkConnectImpl::SetTechnologyEnabled(
    const NetworkTypePattern& technology,
    bool enabled_state) {
  std::string log_string = base::StringPrintf(
      "technology %s, target state: %s", technology.ToDebugString().c_str(),
      (enabled_state ? "ENABLED" : "DISABLED"));
  NET_LOG_USER("SetTechnologyEnabled", log_string);
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  bool enabled = handler->IsTechnologyEnabled(technology);
  if (enabled_state == enabled) {
    NET_LOG_USER("Technology already in target state.", log_string);
    return;
  }
  if (enabled) {
    // User requested to disable the technology.
    handler->SetTechnologyEnabled(technology, false,
                                  network_handler::ErrorCallback());
    return;
  }
  // If we're dealing with a cellular network, then handle SIM lock here.
  // SIM locking only applies to cellular.
  if (technology.MatchesPattern(NetworkTypePattern::Cellular())) {
    const DeviceState* mobile = handler->GetDeviceStateByType(technology);
    if (!mobile) {
      NET_LOG_ERROR("SetTechnologyEnabled with no device", log_string);
      return;
    }
    if (mobile->IsSimAbsent()) {
      // If this is true, then we have a cellular device with no SIM
      // inserted. TODO(armansito): Chrome should display a notification here,
      // prompting the user to insert a SIM card and restart the device to
      // enable cellular. See crbug.com/125171.
      NET_LOG_USER("Cannot enable cellular device without SIM.", log_string);
      return;
    }
    if (!mobile->IsSimLocked()) {
      // A SIM has been inserted, but it is locked. Let the user unlock it
      // via Settings or the details dialog.
      const NetworkState* network = handler->FirstNetworkByType(technology);
      delegate_->ShowNetworkSettings(network ? network->guid() : "");
      return;
    }
  }
  handler->SetTechnologyEnabled(technology, true,
                                network_handler::ErrorCallback());
}

void NetworkConnectImpl::ActivateCellular(const std::string& network_id) {
  NET_LOG_USER("ActivateCellular", network_id);
  const NetworkState* cellular = GetNetworkStateFromId(network_id);
  if (!cellular || cellular->type() != shill::kTypeCellular) {
    NET_LOG_ERROR("ActivateCellular with no Service", network_id);
    return;
  }
  // Cellular activation now always goes through an online portal shown by the
  // mobile setup dialog.
  ShowMobileSetup(network_id);
}

void NetworkConnectImpl::ShowMobileSetup(const std::string& network_id) {
  const NetworkState* cellular = GetNetworkStateFromId(network_id);
  if (!cellular || cellular->type() != shill::kTypeCellular) {
    NET_LOG_ERROR("ShowMobileSetup without Cellular network", network_id);
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

void NetworkConnectImpl::ConfigureNetworkIdAndConnect(
    const std::string& network_id,
    const base::DictionaryValue& properties,
    bool shared) {
  NET_LOG_USER("ConfigureNetworkIdAndConnect", network_id);

  std::unique_ptr<base::DictionaryValue> properties_to_set(
      properties.DeepCopy());

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
      base::Bind(&NetworkConnectImpl::ConfigureSetProfileSucceeded,
                 weak_factory_.GetWeakPtr(), network_id,
                 base::Passed(&properties_to_set)),
      base::Bind(&NetworkConnectImpl::SetPropertiesFailed,
                 weak_factory_.GetWeakPtr(), "SetProfile: " + profile_path,
                 network_id));
}

void NetworkConnectImpl::CreateConfigurationAndConnect(
    base::DictionaryValue* properties,
    bool shared) {
  NET_LOG_USER("CreateConfigurationAndConnect", "");
  CallCreateConfiguration(properties, shared, true /* connect_on_configure */);
}

void NetworkConnectImpl::CreateConfiguration(base::DictionaryValue* properties,
                                             bool shared) {
  NET_LOG_USER("CreateConfiguration", "");
  CallCreateConfiguration(properties, shared, false /* connect_on_configure */);
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

}  // namespace chromeos
