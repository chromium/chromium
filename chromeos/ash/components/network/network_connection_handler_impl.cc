// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_connection_handler_impl.h"

#include <memory>
#include <ostream>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/auto_connect_handler.h"
#include "chromeos/ash/components/network/cellular_connection_handler.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/client_cert_resolver.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/ash/components/network/prohibited_technologies_handler.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "dbus/object_path.h"
#include "net/cert/x509_certificate.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// If connection to a network that may require a client certificate is requested
// when client certificates are not loaded yet, wait this long until
// certificates have been loaded.
constexpr base::TimeDelta kMaxCertLoadTimeSeconds = base::Seconds(15);

// Timeout after which a pending cellular connect request is considered failed.
constexpr base::TimeDelta kCellularConnectTimeout = base::Seconds(150);

const char kCellularConfigurationFailureHistogramName[] =
    "Network.Ash.Cellular.ConfigurationFailure.Type";

bool IsAuthenticationError(const std::string& error) {
  return (error == shill::kErrorBadWEPKey ||
          error == shill::kErrorPppAuthFailed ||
          error == shill::kErrorEapLocalTlsFailed ||
          error == shill::kErrorEapRemoteTlsFailed ||
          error == shill::kErrorEapAuthenticationFailed);
}

std::string GetStringFromDictionary(const base::Value::Dict& dict,
                                    const std::string& key) {
  const std::string* s = dict.FindString(key);
  return s ? *s : std::string();
}

bool IsCertificateConfigured(const client_cert::ConfigType cert_config_type,
                             const base::Value::Dict& properties) {
  // VPN certificate properties are read from the Provider dictionary.
  const base::Value::Dict* provider_properties =
      properties.FindDict(shill::kProviderProperty);
  switch (cert_config_type) {
    case client_cert::ConfigType::kNone:
      return true;
    case client_cert::ConfigType::kOpenVpn:
      // We don't know whether a passphrase or certificates are required, so
      // always return true here (otherwise we will never attempt to connect).
      // TODO(stevenjb/cernekee): Fix this?
      return true;
    case client_cert::ConfigType::kL2tpIpsec: {
      if (!provider_properties)
        return false;

      std::string client_cert_id = GetStringFromDictionary(
          *provider_properties, shill::kL2TPIPsecClientCertIdProperty);
      return !client_cert_id.empty();
    }
    case client_cert::ConfigType::kEap: {
      std::string cert_id =
          GetStringFromDictionary(properties, shill::kEapCertIdProperty);
      std::string key_id =
          GetStringFromDictionary(properties, shill::kEapKeyIdProperty);
      std::string identity =
          GetStringFromDictionary(properties, shill::kEapIdentityProperty);
      return !cert_id.empty() && !key_id.empty() && !identity.empty();
    }
    case client_cert::ConfigType::kIkev2: {
      if (!provider_properties)
        return false;

      std::string client_cert_id = GetStringFromDictionary(
          *provider_properties, shill::kIKEv2ClientCertIdProperty);
      return !client_cert_id.empty();
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

std::string VPNCheckCredentials(const std::string& service_path,
                                const std::string& provider_type,
                                const base::Value::Dict& provider_properties) {
  if (provider_type == shill::kProviderOpenVpn) {
    bool passphrase_required =
        provider_properties.FindBool(shill::kPassphraseRequiredProperty)
            .value_or(false);
    if (passphrase_required) {
      NET_LOG(ERROR) << "OpenVPN: Passphrase Required for: "
                     << NetworkPathId(service_path);
      return NetworkConnectionHandler::kErrorPassphraseRequired;
    }
    NET_LOG(EVENT) << "OpenVPN Is Configured: " << NetworkPathId(service_path);
  } else {
    bool passphrase_required =
        provider_properties.FindBool(shill::kL2TPIPsecPskRequiredProperty)
            .value_or(false);
    if (passphrase_required) {
      NET_LOG(ERROR) << "VPN: PSK Required for: "
                     << NetworkPathId(service_path);
      return NetworkConnectionHandler::kErrorConfigurationRequired;
    }
    passphrase_required =
        provider_properties.FindBool(shill::kPassphraseRequiredProperty)
            .value_or(false);
    if (passphrase_required) {
      NET_LOG(ERROR) << "VPN: Passphrase Required for: "
                     << NetworkPathId(service_path);
      return NetworkConnectionHandler::kErrorPassphraseRequired;
    }
    NET_LOG(EVENT) << "VPN Is Configured: " << NetworkPathId(service_path);
  }
  return std::string();
}

bool ShouldUseSharedProfileByDefault(const NetworkState* network) {
  if (!LoginState::Get()->UserHasNetworkProfile()) {
    // The session type has no network profile, so use the shared profile.
    // TODO - b/295521307: This branch may become unnecessary when Managed Guest
    // Sessions also have a network profile.
    return true;
  }
  if (LoginState::Get()->IsGuestSessionUser()) {
    // Prefer the "user" profile for guest sessions, even for open WiFi
    // networks. This must come before the next branch. See b/307555248 for more
    // details.
    return false;
  }
  // Cellular networks and insecure Wi-Fi networks are persisted in the shared
  // profile by default.
  if (network) {
    if (network->type() == shill::kTypeCellular) {
      return true;
    }
    if (network->type() == shill::kTypeWifi) {
      return !network->IsSecure();
    }
  }
  // Use the user profile if available.
  return false;
}

std::string GetDefaultUserProfilePath(const NetworkState* network) {
  DCHECK(LoginState::IsInitialized());

  if (ShouldUseSharedProfileByDefault(network) ||
      !NetworkHandler::IsInitialized()) {
    return NetworkProfileHandler::GetSharedProfilePath();
  }
  const NetworkProfile* profile =
      NetworkHandler::Get()->network_profile_handler()->GetDefaultUserProfile();
  return profile ? profile->path
                 : NetworkProfileHandler::GetSharedProfilePath();
}

bool IsVpnProhibited() {
  if (!NetworkHandler::IsInitialized())
    return false;

  std::vector<std::string> prohibited_technologies =
      NetworkHandler::Get()
          ->prohibited_technologies_handler()
          ->GetCurrentlyProhibitedTechnologies();
  return base::Contains(prohibited_technologies, shill::kTypeVPN);
}

bool IsBuiltInVpnType(const std::string& vpn_type) {
  return vpn_type != shill::kProviderArcVpn &&
         vpn_type != shill::kProviderThirdPartyVpn;
}

std::ostream& operator<<(std::ostream& stream, client_cert::ConfigType type) {
  switch (type) {
    case client_cert::ConfigType::kNone:
      stream << "None";
      return stream;
    case client_cert::ConfigType::kOpenVpn:
      stream << "OpenVPN";
      return stream;
    case client_cert::ConfigType::kL2tpIpsec:
      stream << "L2TP/IPsec";
      return stream;
    case client_cert::ConfigType::kIkev2:
      stream << "IKEv2";
      return stream;
    case client_cert::ConfigType::kEap:
      stream << "EAP";
      return stream;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

NetworkConnectionHandlerImpl::ConnectRequest::ConnectRequest(
    ConnectCallbackMode mode,
    const std::string& service_path,
    const std::string& profile_path,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error)
    : mode(mode),
      service_path(service_path),
      profile_path(profile_path),
      connect_state(CONNECT_REQUESTED),
      success_callback(std::move(success_callback)),
      error_callback(std::move(error)) {}

NetworkConnectionHandlerImpl::ConnectRequest::~ConnectRequest() = default;

NetworkConnectionHandlerImpl::ConnectRequest::ConnectRequest(ConnectRequest&&) =
    default;

NetworkConnectionHandlerImpl::NetworkConnectionHandlerImpl() = default;

NetworkConnectionHandlerImpl::~NetworkConnectionHandlerImpl() {
  if (network_cert_loader_)
    network_cert_loader_->RemoveObserver(this);
}

void NetworkConnectionHandlerImpl::Init(
    NetworkStateHandler* network_state_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
    CellularConnectionHandler* cellular_connection_handler) {
  if (NetworkCertLoader::IsInitialized()) {
    network_cert_loader_ = NetworkCertLoader::Get();
    network_cert_loader_->AddObserver(this);
    if (network_cert_loader_->initial_load_finished()) {
      NET_LOG(EVENT) << "Certificates Loaded";
      certificates_loaded_ = true;
    }
  } else {
    // TODO(tbarzic): Require a mock or stub |network_cert_loader| in tests.
    NET_LOG(DEBUG) << "Certificate Loader not initialized";
    certificates_loaded_ = true;
  }

  if (network_state_handler) {
    network_state_handler_ = network_state_handler;
    network_state_handler_observer_.Observe(network_state_handler_.get());
  }
  configuration_handler_ = network_configuration_handler;
  managed_configuration_handler_ = managed_network_configuration_handler;
  cellular_connection_handler_ = cellular_connection_handler;

  // After this point, the NetworkConnectionHandlerImpl is fully initialized
  // (all handler references set, observers registered, ...).
}

void NetworkConnectionHandlerImpl::OnCertificatesLoaded() {
  certificates_loaded_ = true;
  NET_LOG(EVENT) << "Certificates Loaded";
  if (queued_connect_)
    ConnectToQueuedNetwork();
}

void NetworkConnectionHandlerImpl::OnAutoConnectedInitiated(
    int auto_connect_reasons) {
  const int kManagedNetworkReasonsBitmask =
      AutoConnectHandler::AUTO_CONNECT_REASON_POLICY_APPLIED |
      AutoConnectHandler::AUTO_CONNECT_REASON_CERTIFICATE_RESOLVED;
  if (auto_connect_reasons & kManagedNetworkReasonsBitmask) {
    has_policy_auto_connect_ = true;
  }
}

void NetworkConnectionHandlerImpl::ConnectToNetwork(
    const std::string& service_path,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback,
    bool check_error_state,
    ConnectCallbackMode mode) {
  NET_LOG(USER) << "ConnectToNetworkRequested: " << NetworkPathId(service_path);
  for (auto& observer : observers_)
    observer.ConnectToNetworkRequested(service_path);

  // Clear any existing queued connect request.
  if (queued_connect_) {
    network_state_handler_->SetNetworkConnectRequested(
        queued_connect_->service_path, false);
    queued_connect_.reset();
  }

  if (HasConnectingNetwork(service_path)) {
    NET_LOG(USER) << "Connect Request while pending: "
                  << NetworkPathId(service_path);
    InvokeConnectErrorCallback(service_path, std::move(error_callback),
                               kErrorConnecting);
    return;
  }

  // Check cached network state for connected, connecting, or unactivated
  // networks. These states will not be affected by a recent configuration.
  // Note: NetworkState may not exist for a network that was recently
  // configured, in which case these checks do not apply anyway.
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  // Starts as empty string and is set only when a cellular ICCID is present.
  std::string cellular_network_iccid;

  if (network) {
    // For existing networks, perform some immediate consistency checks.
    const std::string connection_state = network->connection_state();
    if (NetworkState::StateIsConnected(connection_state)) {
      NET_LOG(ERROR) << "Connect Request while connected: "
                     << NetworkId(network);
      InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                 kErrorConnected);
      return;
    }
    if (NetworkState::StateIsConnecting(connection_state)) {
      InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                 kErrorConnecting);
      return;
    }

    if (check_error_state) {
      const std::string& error = network->GetError();
      if (error == shill::kErrorBadPassphrase) {
        InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                   kErrorBadPassphrase);
        return;
      }
      if (IsAuthenticationError(error)) {
        InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                   kErrorAuthenticationRequired);
        return;
      }
    }

    if (NetworkTypePattern::Tether().MatchesType(network->type())) {
      if (tether_delegate_) {
        const std::string& tether_network_guid = network->guid();
        DCHECK(!tether_network_guid.empty());
        InitiateTetherNetworkConnection(tether_network_guid,
                                        std::move(success_callback),
                                        std::move(error_callback));
      } else {
        InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                   kErrorTetherAttemptWithNoDelegate);
      }
      return;
    }

    if (NetworkTypePattern::VPN().MatchesType(network->type()) &&
        IsBuiltInVpnType(network->GetVpnProviderType()) && IsVpnProhibited()) {
      InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                 kErrorBlockedByPolicy);
      return;
    }

    if (NetworkTypePattern::Cellular().MatchesType(network->type())) {
      if (network->cellular_out_of_credits()) {
        InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                   kErrorCellularOutOfCredits);
        return;
      }

      // Reject request if a cellular connect request is already in progress.
      // This prevents complexity with switching slots when one is already in
      // progress.
      if (HasPendingCellularRequest()) {
        InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                   kErrorCellularDeviceBusy);
        return;
      }

      const DeviceState* cellular_device =
          network_state_handler_->GetDeviceState(network->device_path());

      if (cellular_device &&
          cellular_utils::IsSimPrimary(network->iccid(), cellular_device)) {
        if (cellular_device->IsSimLocked()) {
          // If device is carrier locked, it could be unlocked only by the
          // carrier, so notification to the user is different from the case
          // where where SIM is locked using PIN/PUK code.
          if (cellular_device->IsSimCarrierLocked()) {
            InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                       kErrorSimCarrierLocked);
            return;
          }
          // If the SIM is active and the active SIM is locked, we are
          // attempting to connect to a locked SIM. A SIM must be unlocked
          // before a connection can succeed.
          InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                     kErrorSimPinPukLocked);
          return;
        }
      }

      cellular_network_iccid = network->iccid();
    }
  }

  // If the network does not have a profile path, specify the correct default
  // profile here and set it once connected. Otherwise leave it empty to
  // indicate that it does not need to be set.
  std::string profile_path;
  if (!network || network->profile_path().empty())
    profile_path = GetDefaultUserProfilePath(network);

  bool can_call_connect = false;

  // Connect immediately to 'connectable' networks.
  // TODO(stevenjb): Shill needs to properly set Connectable for VPN.
  if (network && network->connectable() && network->type() != shill::kTypeVPN) {
    if (network->blocked_by_policy()) {
      InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                 kErrorBlockedByPolicy);
      return;
    }

    can_call_connect = true;
  }

  // All synchronous checks passed, add |service_path| to connecting list.
  pending_requests_.emplace(service_path, std::make_unique<ConnectRequest>(
                                              mode, service_path, profile_path,
                                              std::move(success_callback),
                                              std::move(error_callback)));

  // Indicate that a connect was requested. This will be updated by
  // NetworkStateHandler when the connection state changes, or cleared if
  // an error occurs before a connect is initialted.
  network_state_handler_->SetNetworkConnectRequested(service_path, true);

  if (cellular_connection_handler_ && !cellular_network_iccid.empty()) {
    StartConnectTimer(service_path, kCellularConnectTimeout);

    // Cellular networks require special handling before Shill can initiate a
    // connection. Prepare the network for connection before proceeding.
    cellular_connection_handler_->PrepareExistingCellularNetworkForConnection(
        cellular_network_iccid,
        base::BindOnce(&NetworkConnectionHandlerImpl::
                           OnPrepareCellularNetworkForConnectionSuccess,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&NetworkConnectionHandlerImpl::
                           OnPrepareCellularNetworkForConnectionFailure,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (can_call_connect) {
    CallShillConnect(service_path);
    return;
  }

  // Request additional properties to check. VerifyConfiguredAndConnect will
  // use only these properties, not cached properties, to ensure that they
  // are up to date after any recent configuration.
  configuration_handler_->GetShillProperties(
      service_path,
      base::BindOnce(&NetworkConnectionHandlerImpl::VerifyConfiguredAndConnect,
                     weak_ptr_factory_.GetWeakPtr(), check_error_state));
}

void NetworkConnectionHandlerImpl::OnPrepareCellularNetworkForConnectionSuccess(
    const std::string& service_path,
    bool auto_connected) {
  // If the cellular network is auto-connected by Shill, there is no need to
  // call Shill connect.
  if (auto_connected) {
    HandleShillConnectSuccess(service_path);
    return;
  }
  CallShillConnect(service_path);
}

void NetworkConnectionHandlerImpl::DisconnectNetwork(
    const std::string& service_path,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "DisconnectNetwork";
  for (auto& observer : observers_)
    observer.DisconnectRequested(service_path);

  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network) {
    NET_LOG(ERROR) << "Disconnect Error: Not Found: "
                   << NetworkPathId(service_path);
    network_handler::RunErrorCallback(std::move(error_callback),
                                      kErrorNotFound);
    return;
  }
  const std::string connection_state = network->connection_state();
  if (!NetworkState::StateIsConnected(connection_state) &&
      !NetworkState::StateIsConnecting(connection_state) &&
      !GetPendingRequest(service_path)) {
    NET_LOG(ERROR) << "Disconnect Error: Not Connected: " << NetworkId(network);
    network_handler::RunErrorCallback(std::move(error_callback),
                                      kErrorNotConnected);
    return;
  }
  if (NetworkTypePattern::Tether().MatchesType(network->type())) {
    if (tether_delegate_) {
      const std::string& tether_network_guid = network->guid();
      DCHECK(!tether_network_guid.empty());
      InitiateTetherNetworkDisconnection(tether_network_guid,
                                         std::move(success_callback),
                                         std::move(error_callback));
    } else {
      InvokeConnectErrorCallback(service_path, std::move(error_callback),
                                 kErrorTetherAttemptWithNoDelegate);
    }
    return;
  }
  ClearPendingRequest(service_path);
  CallShillDisconnect(service_path, std::move(success_callback),
                      std::move(error_callback));
}

void NetworkConnectionHandlerImpl::NetworkListChanged() {
  CheckAllPendingRequests();
}

void NetworkConnectionHandlerImpl::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (HasConnectingNetwork(network->path()))
    CheckPendingRequest(network->path());
}

void NetworkConnectionHandlerImpl::NetworkIdentifierTransitioned(
    const std::string& old_service_path,
    const std::string& new_service_path,
    const std::string& old_guid,
    const std::string& new_guid) {
  auto it = pending_requests_.find(old_service_path);

  // If the service path transition does not apply to any networks which have
  // pending network requests, there is nothing to update.
  if (it == pending_requests_.end())
    return;

  NET_LOG(EVENT) << "ConnectRequest cache updated service path: "
                 << old_service_path << " => " << new_service_path;

  const NetworkState* network =
      network_state_handler_->GetNetworkState(new_service_path);
  std::string profile_path;
  if (!network || network->profile_path().empty())
    profile_path = GetDefaultUserProfilePath(network);

  // Remove the old map entry from the previous service path and add a new
  // mapping with the updated service path.
  std::unique_ptr<ConnectRequest> request = std::move(it->second);
  request->service_path = new_service_path;
  request->profile_path = profile_path;
  pending_requests_.erase(it);
  pending_requests_.emplace(new_service_path, std::move(request));

  network_state_handler_->SetNetworkConnectRequested(
      new_service_path, /*connect_requested=*/true);
}

bool NetworkConnectionHandlerImpl::HasConnectingNetwork(
    const std::string& service_path) {
  return pending_requests_.count(service_path) != 0;
}

NetworkConnectionHandlerImpl::ConnectRequest*
NetworkConnectionHandlerImpl::GetPendingRequest(
    const std::string& service_path) {
  std::map<std::string, std::unique_ptr<ConnectRequest>>::iterator iter =
      pending_requests_.find(service_path);
  return iter != pending_requests_.end() ? iter->second.get() : nullptr;
}

bool NetworkConnectionHandlerImpl::HasPendingCellularRequest() const {
  return base::ranges::any_of(
      pending_requests_,
      [&](const std::pair<const std::string, std::unique_ptr<ConnectRequest>>&
              pair) {
        const NetworkState* network =
            network_state_handler_->GetNetworkState(pair.first);
        return network && network->Matches(NetworkTypePattern::Cellular());
      });
}

void NetworkConnectionHandlerImpl::OnPrepareCellularNetworkForConnectionFailure(
    const std::string& service_path,
    const std::string& error_name) {
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG(ERROR) << "OnPrepareCellularNetworkForConnectionFailure called "
                      "with no pending "
                   << " request: " << NetworkPathId(service_path);
    return;
  }
  network_handler::ErrorCallback error_callback =
      std::move(request->error_callback);
  ClearPendingRequest(service_path);
  InvokeConnectErrorCallback(service_path, std::move(error_callback),
                             error_name);
}

void NetworkConnectionHandlerImpl::StartConnectTimer(
    const std::string& service_path,
    base::TimeDelta timeout) {
  ConnectRequest* request = GetPendingRequest(service_path);
  DCHECK(request);

  request->timer = std::make_unique<base::OneShotTimer>();
  request->timer->Start(
      FROM_HERE, timeout,
      base::BindOnce(&NetworkConnectionHandlerImpl::OnConnectTimeout,
                     weak_ptr_factory_.GetWeakPtr(), request));
}

void NetworkConnectionHandlerImpl::OnConnectTimeout(ConnectRequest* request) {
  // Copy service path since request will be deleted in ClearPendingRequest.
  std::string service_path = request->service_path;
  NET_LOG(EVENT) << "Connect request timed out for path=" << service_path;
  InvokeConnectErrorCallback(service_path, std::move(request->error_callback),
                             kErrorConnectTimeout);
  ClearPendingRequest(service_path);
}

// ConnectToNetwork implementation

void NetworkConnectionHandlerImpl::VerifyConfiguredAndConnect(
    bool check_error_state,
    const std::string& service_path,
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    HandleConfigurationFailure(
        service_path, "GetShillProperties failed",
        CellularConfigurationFailureType::kFailureGetShillProperties);
    return;
  }
  NET_LOG(EVENT) << "VerifyConfiguredAndConnect: "
                 << NetworkPathId(service_path)
                 << " check_error_state: " << check_error_state;

  // If 'passphrase_required' is still true, then the 'Passphrase' property
  // has not been set to a minimum length value.
  bool passphrase_required =
      properties->FindBool(shill::kPassphraseRequiredProperty).value_or(false);
  if (passphrase_required) {
    ErrorCallbackForPendingRequest(service_path, kErrorPassphraseRequired);
    return;
  }

  const std::string* type = properties->FindString(shill::kTypeProperty);
  if (!type) {
    HandleConfigurationFailure(
        service_path, "Properties with no type",
        CellularConfigurationFailureType::kFailurePropertiesWithNoType);
    return;
  }
  bool connectable =
      properties->FindBool(shill::kConnectableProperty).value_or(false);

  // In case NetworkState was not available in ConnectToNetwork (e.g. it had
  // been recently configured), we need to check Connectable again.
  if (connectable && *type != shill::kTypeVPN) {
    // TODO(stevenjb): Shill needs to properly set Connectable for VPN.
    CallShillConnect(service_path);
    return;
  }

  // Get VPN provider type and host (required for configuration) and ensure
  // that required VPN non-cert properties are set.
  const base::Value::Dict* provider_properties =
      properties->FindDict(shill::kProviderProperty);
  std::string vpn_provider_type, vpn_provider_host, vpn_client_cert_id;
  if (*type == shill::kTypeVPN) {
    // VPN Provider values are read from the "Provider" dictionary, not the
    // "Provider.Type", etc keys (which are used only to set the values).
    if (provider_properties) {
      vpn_provider_type =
          GetStringFromDictionary(*provider_properties, shill::kTypeProperty);
      vpn_provider_host =
          GetStringFromDictionary(*provider_properties, shill::kHostProperty);
      vpn_client_cert_id = GetStringFromDictionary(
          *provider_properties, shill::kL2TPIPsecClientCertIdProperty);
    }
    if (vpn_provider_type.empty() || vpn_provider_host.empty()) {
      NET_LOG(ERROR) << "VPN Provider missing for: "
                     << NetworkPathId(service_path);
      ErrorCallbackForPendingRequest(service_path, kErrorConfigurationRequired);
      return;
    }
  }

  const std::string* guid = properties->FindString(shill::kGuidProperty);
  const std::string* profile = properties->FindString(shill::kProfileProperty);
  ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE;
  const base::Value::Dict* policy = nullptr;
  if (guid && profile) {
    // Fetch network policy with PolicyType::kOriginal to be able to process a
    // client certificate pattern (which would be replaced with a certificate
    // reference in PolicyType::kWithRuntimeValues).
    policy = managed_configuration_handler_->FindPolicyByGuidAndProfile(
        *guid, *profile,
        ManagedNetworkConfigurationHandler::PolicyType::kOriginal, &onc_source,
        /*out_userhash=*/nullptr);
  }
  // Check if network is blocked by policy.
  if (*type == shill::kTypeWifi &&
      onc_source != ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY &&
      onc_source != ::onc::ONCSource::ONC_SOURCE_USER_POLICY) {
    const std::string* hex_ssid = properties->FindString(shill::kWifiHexSsid);
    if (!hex_ssid) {
      ErrorCallbackForPendingRequest(service_path, kErrorHexSsidRequired);
      return;
    }
    if (network_state_handler_->OnlyManagedWifiNetworksAllowed() ||
        base::Contains(managed_configuration_handler_->GetBlockedHexSSIDs(),
                       *hex_ssid)) {
      ErrorCallbackForPendingRequest(service_path, kErrorBlockedByPolicy);
      return;
    }
  }

  client_cert::ClientCertConfig cert_config_from_policy;
  if (policy) {
    client_cert::OncToClientCertConfig(onc_source, *policy,
                                       &cert_config_from_policy);
  }

  client_cert::ConfigType client_cert_type = client_cert::ConfigType::kNone;
  if (*type == shill::kTypeVPN) {
    // Check the VPN types that may use the client cert: OpenVPN, L2TP/IPsec,
    // and IKEv2.
    if (vpn_provider_type == shill::kProviderOpenVpn) {
      client_cert_type = client_cert::ConfigType::kOpenVpn;
    } else if (vpn_provider_type == shill::kProviderL2tpIpsec) {
      // L2TP/IPSec only requires a certificate if one is specified in ONC
      // or one was configured by the UI. Otherwise it is L2TP/IPSec with
      // PSK and doesn't require a certificate.
      if (!vpn_client_cert_id.empty() ||
          cert_config_from_policy.client_cert_type !=
              ::onc::client_cert::kClientCertTypeNone) {
        client_cert_type = client_cert::ConfigType::kL2tpIpsec;
      }
    } else if (vpn_provider_type == shill::kProviderIKEv2) {
      const std::string* auth_type =
          properties->FindString(shill::kIKEv2AuthenticationTypeProperty);
      if (auth_type && *auth_type == shill::kIKEv2AuthenticationTypeCert) {
        client_cert_type = client_cert::ConfigType::kIkev2;
      }
    }
  } else if (*type == shill::kTypeWifi) {
    const std::string* security_class =
        properties->FindString(shill::kSecurityClassProperty);
    if (security_class && *security_class == shill::kSecurityClass8021x)
      client_cert_type = client_cert::ConfigType::kEap;
  }

  base::Value::Dict config_properties;
  if (client_cert_type != client_cert::ConfigType::kNone) {
    // Note: if we get here then a certificate *may* be required, so we want
    // to ensure that certificates have loaded successfully before attempting
    // to connect.
    NET_LOG(DEBUG) << "Client cert type for: " << NetworkPathId(service_path)
                   << ": " << client_cert_type;

    if (!network_cert_loader_ ||
        !network_cert_loader_->can_have_client_certificates()) {
      // There can be no certificates in this device state.
      ErrorCallbackForPendingRequest(service_path, kErrorCertificateRequired);
      return;
    }

    // If certificates have not been loaded yet, queue the connect request.
    if (!certificates_loaded_) {
      NET_LOG(EVENT) << "Certificates not loaded for: "
                     << NetworkPathId(service_path);
      QueueConnectRequest(service_path);
      return;
    }

    // Check certificate properties from policy.
    if (cert_config_from_policy.client_cert_type ==
        ::onc::client_cert::kPattern) {
      if (!ClientCertResolver::ResolveClientCertificateSync(
              client_cert_type, cert_config_from_policy, &config_properties)) {
        NET_LOG(ERROR) << "Non matching certificate for: "
                       << NetworkPathId(service_path);
        ErrorCallbackForPendingRequest(service_path, kErrorCertificateRequired);
        return;
      }
    } else if (check_error_state &&
               !IsCertificateConfigured(client_cert_type, properties.value())) {
      // Network may not be configured.
      NET_LOG(ERROR) << "Certificate not configured for: "
                     << NetworkPathId(service_path);
      ErrorCallbackForPendingRequest(service_path, kErrorConfigurationRequired);
      return;
    }
  }

  if (*type == shill::kTypeVPN) {
    // VPN may require a username, and/or passphrase to be set. (Check after
    // ensuring that any required certificates are configured).
    DCHECK(provider_properties);
    std::string error = VPNCheckCredentials(service_path, vpn_provider_type,
                                            *provider_properties);
    if (!error.empty()) {
      ErrorCallbackForPendingRequest(service_path, error);
      return;
    }

    // If it's L2TP/IPsec PSK, there is no properties to configure, so proceed
    // to connect.
    if (client_cert_type == client_cert::ConfigType::kNone) {
      CallShillConnect(service_path);
      return;
    }
  }

  if (!config_properties.empty()) {
    NET_LOG(EVENT) << "Configuring Network: " << NetworkPathId(service_path);
    configuration_handler_->SetShillProperties(
        service_path, config_properties,
        base::BindOnce(&NetworkConnectionHandlerImpl::CallShillConnect,
                       weak_ptr_factory_.GetWeakPtr(), service_path),
        base::BindOnce(
            &NetworkConnectionHandlerImpl::OnSetShillPropertiesFailed,
            weak_ptr_factory_.GetWeakPtr(), service_path));
    return;
  }

  // Cellular networks are not "connectable" if they are not on the active SIM
  // slot. For VPNs, "connectable" is not reliable. In either case, we can still
  // issue Shill a connection request despite the "connectable" property being
  // false.
  bool can_connect_without_connectable =
      *type == shill::kTypeCellular || *type == shill::kTypeVPN;

  if (!can_connect_without_connectable && check_error_state) {
    NET_LOG(ERROR) << "Non-connectable network is unconfigured: "
                   << NetworkPathId(service_path);
    ErrorCallbackForPendingRequest(service_path, kErrorConfigurationRequired);
    return;
  }

  // Otherwise attempt to connect to possibly gain additional error state from
  // Shill (or in case 'Connectable' is improperly set to false).
  CallShillConnect(service_path);
}

void NetworkConnectionHandlerImpl::QueueConnectRequest(
    const std::string& service_path) {
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG(ERROR) << "No pending request to queue: "
                   << NetworkPathId(service_path);
    return;
  }

  NET_LOG(EVENT) << "Connect Request Queued: " << NetworkPathId(service_path);
  queued_connect_ = std::make_unique<ConnectRequest>(
      request->mode, service_path, request->profile_path,
      std::move(request->success_callback), std::move(request->error_callback));
  pending_requests_.erase(service_path);

  // Post a delayed task to check to see if certificates have loaded. If they
  // haven't, and queued_connect_ has not been cleared (e.g. by a successful
  // connect request), cancel the request and notify the user.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NetworkConnectionHandlerImpl::CheckCertificatesLoaded,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxCertLoadTimeSeconds);
}

// Called after a delay to check whether certificates loaded. If they did not
// and we still have a queued network connect request, show an error and clear
// the request.
void NetworkConnectionHandlerImpl::CheckCertificatesLoaded() {
  // Certificates loaded successfully, nothing more to do here.
  if (certificates_loaded_)
    return;

  // If queued_connect_ has been cleared (e.g. another connect request occurred
  // and wasn't queued), do nothing here.
  if (!queued_connect_)
    return;

  // Notify the user that the connect failed, clear the queued network, and
  // clear the connect_requested flag for the NetworkState.
  std::string queued_connect_service_path = queued_connect_->service_path;
  NET_LOG(ERROR) << "Certificate load timeout: "
                 << NetworkPathId(queued_connect_service_path);
  InvokeConnectErrorCallback(queued_connect_->service_path,
                             std::move(queued_connect_->error_callback),
                             kErrorCertLoadTimeout);
  queued_connect_.reset();
  network_state_handler_->SetNetworkConnectRequested(
      queued_connect_service_path, false);
}

void NetworkConnectionHandlerImpl::ConnectToQueuedNetwork() {
  DCHECK(queued_connect_);

  // Make a copy of |queued_connect_| parameters, because |queued_connect_|
  // will get reset at the beginning of |ConnectToNetwork|.
  std::string service_path = queued_connect_->service_path;
  base::OnceClosure success_callback =
      std::move(queued_connect_->success_callback);
  network_handler::ErrorCallback error_callback =
      std::move(queued_connect_->error_callback);

  NET_LOG(EVENT) << "Connecting to Queued Network: "
                 << NetworkPathId(service_path);
  ConnectToNetwork(service_path, std::move(success_callback),
                   std::move(error_callback), false /* check_error_state */,
                   queued_connect_->mode);
}

void NetworkConnectionHandlerImpl::CallShillConnect(
    const std::string& service_path) {
  NET_LOG(EVENT) << "Sending Connect Request to Shill: "
                 << NetworkPathId(service_path);
  network_state_handler_->ClearLastErrorForNetwork(service_path);
  ShillServiceClient::Get()->Connect(
      dbus::ObjectPath(service_path),
      base::BindOnce(&NetworkConnectionHandlerImpl::HandleShillConnectSuccess,
                     weak_ptr_factory_.GetWeakPtr(), service_path),
      base::BindOnce(&NetworkConnectionHandlerImpl::HandleShillConnectFailure,
                     weak_ptr_factory_.GetWeakPtr(), service_path));
}

void NetworkConnectionHandlerImpl::OnSetShillPropertiesFailed(
    const std::string& service_path,
    const std::string& error_name) {
  HandleConfigurationFailure(
      service_path, error_name,
      CellularConfigurationFailureType::kFailureSetShillProperties);
}

void NetworkConnectionHandlerImpl::HandleConfigurationFailure(
    const std::string& service_path,
    const std::string& error_name,
    CellularConfigurationFailureType failure_type) {
  NET_LOG(ERROR) << "Connect configuration failure: " << error_name
                 << " for: " << NetworkPathId(service_path);
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG(ERROR)
        << "HandleConfigurationFailure called with no pending request: "
        << NetworkPathId(service_path);
    return;
  }

  LogConfigurationFailureTypeIfCellular(service_path, failure_type);

  network_handler::ErrorCallback error_callback =
      std::move(request->error_callback);
  ClearPendingRequest(service_path);
  InvokeConnectErrorCallback(service_path, std::move(error_callback),
                             kErrorConfigureFailed);
}

void NetworkConnectionHandlerImpl::LogConfigurationFailureTypeIfCellular(
    const std::string& service_path,
    CellularConfigurationFailureType failure_type) {
  if (!network_state_handler_)
    return;

  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (!network_state || network_state->GetNetworkTechnologyType() !=
                            NetworkState::NetworkTechnologyType::kCellular) {
    return;
  }

  base::UmaHistogramEnumeration(kCellularConfigurationFailureHistogramName,
                                failure_type);
}

void NetworkConnectionHandlerImpl::HandleShillConnectSuccess(
    const std::string& service_path) {
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG(ERROR)
        << "HandleShillConnectSuccess called with no pending request: "
        << NetworkPathId(service_path);
    return;
  }

  HandleNetworkConnectStarted(request);
  NET_LOG(EVENT) << "Connect Request Acknowledged: "
                 << NetworkPathId(service_path);
  // Do not call success_callback here, wait for one of the following
  // conditions:
  // * State transitions to a non connecting state indicating success or failure
  // * Network is no longer in the visible list, indicating failure
  CheckPendingRequest(service_path);
}

void NetworkConnectionHandlerImpl::HandleShillConnectFailure(
    const std::string& service_path,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG(ERROR)
        << "HandleShillConnectFailure called with no pending request: "
        << NetworkPathId(service_path);
    return;
  }

  // Ignore failure if Shill returns an in progress error. This indicates that a
  // connection attempt is already progress. This connect request will be
  // completed with a success or failure in CheckPendingRequest when the network
  // state changes.
  if (dbus_error_name == shill::kErrorResultInProgress) {
    NET_LOG(DEBUG)
        << "Ignoring connect request in progress error. service_path="
        << service_path;
    // Set connection request to started and check if service has connected.
    HandleNetworkConnectStarted(request);
    CheckPendingRequest(service_path);
    return;
  }

  network_handler::ErrorCallback error_callback =
      std::move(request->error_callback);
  ClearPendingRequest(service_path);

  std::string error;
  if (dbus_error_name == shill::kErrorResultAlreadyConnected) {
    error = kErrorConnected;
  } else {
    network_state_handler_->SetShillConnectError(service_path, dbus_error_name);
    error = kErrorConnectFailed;
  }
  NET_LOG(ERROR) << "Connect Failure: " << NetworkPathId(service_path)
                 << " Error: " << error << " Shill error: " << dbus_error_name;
  InvokeConnectErrorCallback(service_path, std::move(error_callback), error);
}

void NetworkConnectionHandlerImpl::HandleNetworkConnectStarted(
    ConnectRequest* request) {
  if (request->mode == ConnectCallbackMode::ON_STARTED) {
    if (!request->success_callback.is_null())
      std::move(request->success_callback).Run();
    // Request started; do not invoke success or error callbacks on
    // completion.
    request->success_callback.Reset();
    request->error_callback = network_handler::ErrorCallback();
  }
  request->connect_state = ConnectRequest::CONNECT_STARTED;
}

void NetworkConnectionHandlerImpl::CheckPendingRequest(
    const std::string service_path) {
  ConnectRequest* request = GetPendingRequest(service_path);
  DCHECK(request);
  if (request->connect_state == ConnectRequest::CONNECT_REQUESTED)
    return;  // Request has not started, ignore update
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network)
    return;  // NetworkState may not be be updated yet.

  const std::string connection_state = network->connection_state();
  if (NetworkState::StateIsConnecting(connection_state)) {
    request->connect_state = ConnectRequest::CONNECT_CONNECTING;
    return;
  }
  if (NetworkState::StateIsConnected(connection_state)) {
    if (network->type() == shill::kTypeWifi) {
      base::Value::Dict config_properties;
      config_properties.Set(shill::kGuidProperty, network->guid());
      configuration_handler_->SetShillProperties(
          service_path, config_properties, base::DoNothing(),
          network_handler::ErrorCallback());
    }
    if (!request->profile_path.empty()) {
      // If a profile path was specified, set it on a successful connection.
      configuration_handler_->SetNetworkProfile(
          service_path, request->profile_path, base::DoNothing(),
          network_handler::ErrorCallback());
    }
    InvokeConnectSuccessCallback(request->service_path,
                                 std::move(request->success_callback));
    ClearPendingRequest(service_path);
    return;
  }
  if (connection_state == shill::kStateIdle &&
      request->connect_state != ConnectRequest::CONNECT_CONNECTING) {
    // Connection hasn't started yet, keep waiting.
    return;
  }

  // Network is neither connecting or connected; an error occurred.
  std::string error_name;  // 'Canceled' or 'Failed'
  if (connection_state == shill::kStateIdle &&
      (pending_requests_.size() > 1 || has_policy_auto_connect_)) {
    // Another connect request or policy connection canceled this one.
    error_name = kErrorConnectCanceled;
    if (has_policy_auto_connect_) {
      NET_LOG(EVENT) << "Connection request of " << NetworkPathId(service_path)
                     << " got cancelled by policy auto-connect";
      has_policy_auto_connect_ = false;
    }
  } else {
    error_name = kErrorConnectFailed;
    if (connection_state != shill::kStateFailure)
      NET_LOG(ERROR) << "Unexpected State: " << connection_state
                     << " for: " << NetworkPathId(service_path);
  }

  network_handler::ErrorCallback error_callback =
      std::move(request->error_callback);
  ClearPendingRequest(service_path);
  InvokeConnectErrorCallback(service_path, std::move(error_callback),
                             error_name);
}

void NetworkConnectionHandlerImpl::CheckAllPendingRequests() {
  for (std::map<std::string, std::unique_ptr<ConnectRequest>>::iterator iter =
           pending_requests_.begin();
       iter != pending_requests_.end(); ++iter) {
    CheckPendingRequest(iter->first);
  }
}

void NetworkConnectionHandlerImpl::ClearPendingRequest(
    const std::string& service_path) {
  pending_requests_.erase(service_path);
  network_state_handler_->SetNetworkConnectRequested(service_path, false);
}

// Connect callbacks

void NetworkConnectionHandlerImpl::ErrorCallbackForPendingRequest(
    const std::string& service_path,
    const std::string& error_name) {
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG(ERROR) << "ErrorCallbackForPendingRequest with no pending request: "
                   << NetworkPathId(service_path);
    return;
  }
  // Remove the entry before invoking the callback in case it triggers a retry.
  network_handler::ErrorCallback error_callback =
      std::move(request->error_callback);
  ClearPendingRequest(service_path);

  InvokeConnectErrorCallback(service_path, std::move(error_callback),
                             error_name);
}

// Disconnect

void NetworkConnectionHandlerImpl::CallShillDisconnect(
    const std::string& service_path,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "Disconnect Request: " << NetworkPathId(service_path);
  ShillServiceClient::Get()->Disconnect(
      dbus::ObjectPath(service_path),
      base::BindOnce(
          &NetworkConnectionHandlerImpl::HandleShillDisconnectSuccess,
          weak_ptr_factory_.GetWeakPtr(), service_path,
          std::move(success_callback)),
      base::BindOnce(&network_handler::ShillErrorCallbackFunction,
                     kErrorDisconnectFailed, service_path,
                     std::move(error_callback)));
}

void NetworkConnectionHandlerImpl::HandleShillDisconnectSuccess(
    const std::string& service_path,
    base::OnceClosure success_callback) {
  NET_LOG(EVENT) << "Disconnect Request Sent for: "
                 << NetworkPathId(service_path);
  if (!success_callback.is_null())
    std::move(success_callback).Run();
}

}  // namespace ash
