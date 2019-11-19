// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_connection_handler_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/client_cert_resolver.h"
#include "chromeos/network/client_cert_util.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/shill_property_util.h"
#include "dbus/object_path.h"
#include "net/cert/x509_certificate.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

bool IsAuthenticationError(const std::string& error) {
  return (error == shill::kErrorBadWEPKey ||
          error == shill::kErrorPppAuthFailed ||
          error == shill::kErrorEapLocalTlsFailed ||
          error == shill::kErrorEapRemoteTlsFailed ||
          error == shill::kErrorEapAuthenticationFailed);
}

std::string GetStringFromDictionary(const base::DictionaryValue& dict,
                                    const std::string& key) {
  std::string s;
  dict.GetStringWithoutPathExpansion(key, &s);
  return s;
}

bool IsCertificateConfigured(const client_cert::ConfigType cert_config_type,
                             const base::DictionaryValue& service_properties) {
  // VPN certificate properties are read from the Provider dictionary.
  const base::DictionaryValue* provider_properties = NULL;
  service_properties.GetDictionaryWithoutPathExpansion(shill::kProviderProperty,
                                                       &provider_properties);
  switch (cert_config_type) {
    case client_cert::CONFIG_TYPE_NONE:
      return true;
    case client_cert::CONFIG_TYPE_OPENVPN:
      // We don't know whether a pasphrase or certificates are required, so
      // always return true here (otherwise we will never attempt to connect).
      // TODO(stevenjb/cernekee): Fix this?
      return true;
    case client_cert::CONFIG_TYPE_IPSEC: {
      if (!provider_properties)
        return false;

      std::string client_cert_id;
      provider_properties->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertIdProperty, &client_cert_id);
      return !client_cert_id.empty();
    }
    case client_cert::CONFIG_TYPE_EAP: {
      std::string cert_id = GetStringFromDictionary(service_properties,
                                                    shill::kEapCertIdProperty);
      std::string key_id =
          GetStringFromDictionary(service_properties, shill::kEapKeyIdProperty);
      std::string identity = GetStringFromDictionary(
          service_properties, shill::kEapIdentityProperty);
      return !cert_id.empty() && !key_id.empty() && !identity.empty();
    }
  }
  NOTREACHED();
  return false;
}

std::string VPNCheckCredentials(
    const std::string& service_path,
    const std::string& provider_type,
    const base::DictionaryValue& provider_properties) {
  if (provider_type == shill::kProviderOpenVpn) {
    std::string username;
    provider_properties.GetStringWithoutPathExpansion(
        shill::kOpenVPNUserProperty, &username);
    bool passphrase_required = false;
    provider_properties.GetBooleanWithoutPathExpansion(
        shill::kPassphraseRequiredProperty, &passphrase_required);
    if (passphrase_required) {
      NET_LOG(ERROR) << "OpenVPN: Passphrase Required for: " << service_path;
      return NetworkConnectionHandler::kErrorPassphraseRequired;
    }
    NET_LOG_EVENT("OpenVPN Is Configured", service_path);
  } else {
    bool passphrase_required = false;
    provider_properties.GetBooleanWithoutPathExpansion(
        shill::kL2tpIpsecPskRequiredProperty, &passphrase_required);
    if (passphrase_required) {
      NET_LOG(ERROR) << "VPN: PSK Required for: " << service_path;
      return NetworkConnectionHandler::kErrorConfigurationRequired;
    }
    provider_properties.GetBooleanWithoutPathExpansion(
        shill::kPassphraseRequiredProperty, &passphrase_required);
    if (passphrase_required) {
      NET_LOG(ERROR) << "VPN: Passphrase Required for: " << service_path;
      return NetworkConnectionHandler::kErrorPassphraseRequired;
    }
    NET_LOG(EVENT) << "VPN Is Configured: " << service_path;
  }
  return std::string();
}

std::string GetDefaultUserProfilePath(const NetworkState* network) {
  if (!NetworkHandler::IsInitialized() ||
      (LoginState::IsInitialized() &&
       !LoginState::Get()->UserHasNetworkProfile()) ||
      (network && network->type() == shill::kTypeWifi &&
       network->security_class() == shill::kSecurityNone)) {
    return NetworkProfileHandler::GetSharedProfilePath();
  }
  const NetworkProfile* profile =
      NetworkHandler::Get()->network_profile_handler()->GetDefaultUserProfile();
  return profile ? profile->path
                 : NetworkProfileHandler::GetSharedProfilePath();
}

}  // namespace

NetworkConnectionHandlerImpl::ConnectRequest::ConnectRequest(
    ConnectCallbackMode mode,
    const std::string& service_path,
    const std::string& profile_path,
    const base::Closure& success,
    const network_handler::ErrorCallback& error)
    : mode(mode),
      service_path(service_path),
      profile_path(profile_path),
      connect_state(CONNECT_REQUESTED),
      success_callback(success),
      error_callback(error) {}

NetworkConnectionHandlerImpl::ConnectRequest::~ConnectRequest() = default;

NetworkConnectionHandlerImpl::ConnectRequest::ConnectRequest(
    const ConnectRequest& other) = default;

NetworkConnectionHandlerImpl::NetworkConnectionHandlerImpl()
    : network_cert_loader_(NULL),
      network_state_handler_(NULL),
      configuration_handler_(NULL),
      logged_in_(false),
      certificates_loaded_(false) {}

NetworkConnectionHandlerImpl::~NetworkConnectionHandlerImpl() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (network_cert_loader_)
    network_cert_loader_->RemoveObserver(this);
  if (LoginState::IsInitialized())
    LoginState::Get()->RemoveObserver(this);
}

void NetworkConnectionHandlerImpl::Init(
    NetworkStateHandler* network_state_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  if (NetworkCertLoader::IsInitialized()) {
    network_cert_loader_ = NetworkCertLoader::Get();
    network_cert_loader_->AddObserver(this);
    if (network_cert_loader_->initial_load_finished()) {
      NET_LOG_EVENT("Certificates Loaded", "");
      certificates_loaded_ = true;
    }
  } else {
    // TODO(tbarzic): Require a mock or stub |network_cert_loader| in tests.
    NET_LOG_EVENT("Certificate Loader not initialized", "");
    certificates_loaded_ = true;
  }

  if (network_state_handler) {
    network_state_handler_ = network_state_handler;
    network_state_handler_->AddObserver(this, FROM_HERE);
  }
  configuration_handler_ = network_configuration_handler;
  managed_configuration_handler_ = managed_network_configuration_handler;

  // After this point, the NetworkConnectionHandlerImpl is fully initialized
  // (all handler references set, observers registered, ...).

  if (LoginState::IsInitialized())
    LoggedInStateChanged();
}

void NetworkConnectionHandlerImpl::LoggedInStateChanged() {
  LoginState* login_state = LoginState::Get();
  if (logged_in_ || !login_state->IsUserLoggedIn())
    return;

  logged_in_ = true;
  logged_in_time_ = base::TimeTicks::Now();
}

void NetworkConnectionHandlerImpl::OnCertificatesLoaded() {
  certificates_loaded_ = true;
  NET_LOG_EVENT("Certificates Loaded", "");
  if (queued_connect_)
    ConnectToQueuedNetwork();
}

void NetworkConnectionHandlerImpl::ConnectToNetwork(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback,
    bool check_error_state,
    ConnectCallbackMode mode) {
  NET_LOG_USER("ConnectToNetworkRequested", service_path);
  for (auto& observer : observers_)
    observer.ConnectToNetworkRequested(service_path);

  // Clear any existing queued connect request.
  if (queued_connect_) {
    network_state_handler_->SetNetworkConnectRequested(
        queued_connect_->service_path, false);
    queued_connect_.reset();
  }

  if (HasConnectingNetwork(service_path)) {
    NET_LOG(USER) << "Connect Request while pending: " << service_path;
    InvokeConnectErrorCallback(service_path, error_callback, kErrorConnecting);
    return;
  }

  // Check cached network state for connected, connecting, or unactivated
  // networks. These states will not be affected by a recent configuration.
  // Note: NetworkState may not exist for a network that was recently
  // configured, in which case these checks do not apply anyway.
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  if (network) {
    // For existing networks, perform some immediate consistency checks.
    const std::string connection_state = network->connection_state();
    if (NetworkState::StateIsConnected(connection_state)) {
      NET_LOG(ERROR) << "Connect Request while connected: " << service_path;
      InvokeConnectErrorCallback(service_path, error_callback, kErrorConnected);
      return;
    }
    if (NetworkState::StateIsConnecting(connection_state)) {
      InvokeConnectErrorCallback(service_path, error_callback,
                                 kErrorConnecting);
      return;
    }

    if (check_error_state) {
      const std::string& error = network->GetError();
      if (error == shill::kErrorBadPassphrase) {
        InvokeConnectErrorCallback(service_path, error_callback,
                                   kErrorBadPassphrase);
        return;
      }
      if (IsAuthenticationError(error)) {
        InvokeConnectErrorCallback(service_path, error_callback,
                                   kErrorAuthenticationRequired);
        return;
      }
    }

    if (NetworkTypePattern::Tether().MatchesType(network->type())) {
      if (tether_delegate_) {
        const std::string& tether_network_guid = network->guid();
        DCHECK(!tether_network_guid.empty());
        InitiateTetherNetworkConnection(tether_network_guid, success_callback,
                                        error_callback);
      } else {
        InvokeConnectErrorCallback(service_path, error_callback,
                                   kErrorTetherAttemptWithNoDelegate);
      }
      return;
    }
  }

  // If the network does not have a profile path, specify the correct default
  // profile here and set it once connected. Otherwise leave it empty to
  // indicate that it does not need to be set.
  std::string profile_path;
  if (!network || network->profile_path().empty())
    profile_path = GetDefaultUserProfilePath(network);

  bool call_connect = false;

  // Connect immediately to 'connectable' networks.
  // TODO(stevenjb): Shill needs to properly set Connectable for VPN.
  if (network && network->connectable() && network->type() != shill::kTypeVPN) {
    if (network->blocked_by_policy()) {
      InvokeConnectErrorCallback(service_path, error_callback,
                                 kErrorBlockedByPolicy);
      return;
    }

    call_connect = true;
  }

  // All synchronous checks passed, add |service_path| to connecting list.
  pending_requests_.emplace(service_path,
                            ConnectRequest(mode, service_path, profile_path,
                                           success_callback, error_callback));

  // Indicate that a connect was requested. This will be updated by
  // NetworkStateHandler when the connection state changes, or cleared if
  // an error occurs before a connect is initialted.
  network_state_handler_->SetNetworkConnectRequested(service_path, true);

  if (call_connect) {
    CallShillConnect(service_path);
    return;
  }

  // Request additional properties to check. VerifyConfiguredAndConnect will
  // use only these properties, not cached properties, to ensure that they
  // are up to date after any recent configuration.
  configuration_handler_->GetShillProperties(
      service_path,
      base::Bind(&NetworkConnectionHandlerImpl::VerifyConfiguredAndConnect,
                 AsWeakPtr(), check_error_state),
      base::Bind(&NetworkConnectionHandlerImpl::HandleConfigurationFailure,
                 AsWeakPtr(), service_path));
}

void NetworkConnectionHandlerImpl::DisconnectNetwork(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG_USER("DisconnectNetwork", service_path);
  for (auto& observer : observers_)
    observer.DisconnectRequested(service_path);

  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network) {
    NET_LOG_ERROR("Disconnect Error: Not Found", service_path);
    network_handler::RunErrorCallback(error_callback, service_path,
                                      kErrorNotFound, "");
    return;
  }
  const std::string connection_state = network->connection_state();
  if (!NetworkState::StateIsConnected(connection_state) &&
      !NetworkState::StateIsConnecting(connection_state)) {
    NET_LOG_ERROR("Disconnect Error: Not Connected", service_path);
    network_handler::RunErrorCallback(error_callback, service_path,
                                      kErrorNotConnected, "");
    return;
  }
  if (NetworkTypePattern::Tether().MatchesType(network->type())) {
    if (tether_delegate_) {
      const std::string& tether_network_guid = network->guid();
      DCHECK(!tether_network_guid.empty());
      InitiateTetherNetworkDisconnection(tether_network_guid, success_callback,
                                         error_callback);
    } else {
      InvokeConnectErrorCallback(service_path, error_callback,
                                 kErrorTetherAttemptWithNoDelegate);
    }
    return;
  }
  ClearPendingRequest(service_path);
  CallShillDisconnect(service_path, success_callback, error_callback);
}

void NetworkConnectionHandlerImpl::NetworkListChanged() {
  CheckAllPendingRequests();
}

void NetworkConnectionHandlerImpl::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (HasConnectingNetwork(network->path()))
    CheckPendingRequest(network->path());
}

bool NetworkConnectionHandlerImpl::HasConnectingNetwork(
    const std::string& service_path) {
  return pending_requests_.count(service_path) != 0;
}

NetworkConnectionHandlerImpl::ConnectRequest*
NetworkConnectionHandlerImpl::GetPendingRequest(
    const std::string& service_path) {
  std::map<std::string, ConnectRequest>::iterator iter =
      pending_requests_.find(service_path);
  return iter != pending_requests_.end() ? &(iter->second) : NULL;
}

// ConnectToNetwork implementation

void NetworkConnectionHandlerImpl::VerifyConfiguredAndConnect(
    bool check_error_state,
    const std::string& service_path,
    const base::DictionaryValue& service_properties) {
  NET_LOG(EVENT) << "VerifyConfiguredAndConnect: " << service_path
                 << " check_error_state: " << check_error_state;

  // If 'passphrase_required' is still true, then the 'Passphrase' property
  // has not been set to a minimum length value.
  bool passphrase_required = false;
  service_properties.GetBooleanWithoutPathExpansion(
      shill::kPassphraseRequiredProperty, &passphrase_required);
  if (passphrase_required) {
    ErrorCallbackForPendingRequest(service_path, kErrorPassphraseRequired);
    return;
  }

  std::string type, security_class;
  service_properties.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  service_properties.GetStringWithoutPathExpansion(
      shill::kSecurityClassProperty, &security_class);
  bool connectable = false;
  service_properties.GetBooleanWithoutPathExpansion(shill::kConnectableProperty,
                                                    &connectable);

  // In case NetworkState was not available in ConnectToNetwork (e.g. it had
  // been recently configured), we need to check Connectable again.
  if (connectable && type != shill::kTypeVPN) {
    // TODO(stevenjb): Shill needs to properly set Connectable for VPN.
    CallShillConnect(service_path);
    return;
  }

  // Get VPN provider type and host (required for configuration) and ensure
  // that required VPN non-cert properties are set.
  const base::DictionaryValue* provider_properties = NULL;
  std::string vpn_provider_type, vpn_provider_host, vpn_client_cert_id;
  if (type == shill::kTypeVPN) {
    // VPN Provider values are read from the "Provider" dictionary, not the
    // "Provider.Type", etc keys (which are used only to set the values).
    if (service_properties.GetDictionaryWithoutPathExpansion(
            shill::kProviderProperty, &provider_properties)) {
      provider_properties->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                                         &vpn_provider_type);
      provider_properties->GetStringWithoutPathExpansion(shill::kHostProperty,
                                                         &vpn_provider_host);
      provider_properties->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertIdProperty, &vpn_client_cert_id);
    }
    if (vpn_provider_type.empty() || vpn_provider_host.empty()) {
      NET_LOG(ERROR) << "VPN Provider missing for: " << service_path;
      ErrorCallbackForPendingRequest(service_path, kErrorConfigurationRequired);
      return;
    }
  }

  std::string guid;
  service_properties.GetStringWithoutPathExpansion(shill::kGuidProperty, &guid);
  std::string profile;
  service_properties.GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                   &profile);
  ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE;
  const base::DictionaryValue* policy =
      managed_configuration_handler_->FindPolicyByGuidAndProfile(guid, profile,
                                                                 &onc_source);

  // Check if network is blocked by policy.
  if (type == shill::kTypeWifi &&
      onc_source != ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY &&
      onc_source != ::onc::ONCSource::ONC_SOURCE_USER_POLICY) {
    const base::Value* hex_ssid_value = service_properties.FindKeyOfType(
        shill::kWifiHexSsid, base::Value::Type::STRING);
    if (!hex_ssid_value) {
      ErrorCallbackForPendingRequest(service_path, kErrorHexSsidRequired);
      return;
    }
    if (network_state_handler_->OnlyManagedWifiNetworksAllowed() ||
        base::Contains(managed_configuration_handler_->GetBlacklistedHexSSIDs(),
                       hex_ssid_value->GetString())) {
      ErrorCallbackForPendingRequest(service_path, kErrorBlockedByPolicy);
      return;
    }
  }

  client_cert::ClientCertConfig cert_config_from_policy;
  if (policy) {
    client_cert::OncToClientCertConfig(onc_source, *policy,
                                       &cert_config_from_policy);
  }

  client_cert::ConfigType client_cert_type = client_cert::CONFIG_TYPE_NONE;
  if (type == shill::kTypeVPN) {
    if (vpn_provider_type == shill::kProviderOpenVpn) {
      client_cert_type = client_cert::CONFIG_TYPE_OPENVPN;
    } else {
      // L2TP/IPSec only requires a certificate if one is specified in ONC
      // or one was configured by the UI. Otherwise it is L2TP/IPSec with
      // PSK and doesn't require a certificate.
      //
      // TODO(benchan): Modify shill to specify the authentication type via
      // the kL2tpIpsecAuthenticationType property, so that Chrome doesn't need
      // to deduce the authentication type based on the
      // kL2tpIpsecClientCertIdProperty here (and also in VPNConfigView).
      if (!vpn_client_cert_id.empty() ||
          cert_config_from_policy.client_cert_type !=
              ::onc::client_cert::kClientCertTypeNone) {
        client_cert_type = client_cert::CONFIG_TYPE_IPSEC;
      }
    }
  } else if (type == shill::kTypeWifi &&
             security_class == shill::kSecurity8021x) {
    client_cert_type = client_cert::CONFIG_TYPE_EAP;
  }

  base::DictionaryValue config_properties;
  if (client_cert_type != client_cert::CONFIG_TYPE_NONE) {
    // Note: if we get here then a certificate *may* be required, so we want
    // to ensure that certificates have loaded successfully before attempting
    // to connect.
    NET_LOG(DEBUG) << "Client cert type for: " << service_path << ": "
                   << client_cert_type;

    // User must be logged in to connect to a network requiring a certificate.
    if (!logged_in_ || !network_cert_loader_) {
      NET_LOG(ERROR) << "User not logged in for: " << service_path;
      ErrorCallbackForPendingRequest(service_path, kErrorCertificateRequired);
      return;
    }
    // If certificates have not been loaded yet, queue the connect request.
    if (!certificates_loaded_) {
      NET_LOG(EVENT) << "Certificates not loaded for: " << service_path;
      QueueConnectRequest(service_path);
      return;
    }

    // Check certificate properties from policy.
    if (cert_config_from_policy.client_cert_type ==
        ::onc::client_cert::kPattern) {
      if (!ClientCertResolver::ResolveClientCertificateSync(
              client_cert_type, cert_config_from_policy, &config_properties)) {
        NET_LOG(ERROR) << "Non matching certificate for: " << service_path;
        ErrorCallbackForPendingRequest(service_path, kErrorCertificateRequired);
        return;
      }
    } else if (check_error_state &&
               !IsCertificateConfigured(client_cert_type, service_properties)) {
      // Network may not be configured.
      NET_LOG(ERROR) << "Certificate not configured for: " << service_path;
      ErrorCallbackForPendingRequest(service_path, kErrorConfigurationRequired);
      return;
    }
  }

  if (type == shill::kTypeVPN) {
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
    if (client_cert_type == client_cert::CONFIG_TYPE_NONE) {
      CallShillConnect(service_path);
      return;
    }
  }

  if (!config_properties.empty()) {
    NET_LOG(EVENT) << "Configuring Network: " << service_path;
    configuration_handler_->SetShillProperties(
        service_path, config_properties,
        base::Bind(&NetworkConnectionHandlerImpl::CallShillConnect, AsWeakPtr(),
                   service_path),
        base::Bind(&NetworkConnectionHandlerImpl::HandleConfigurationFailure,
                   AsWeakPtr(), service_path));
    return;
  }

  if (type != shill::kTypeVPN && check_error_state) {
    // For non VPNs, 'Connectable' must be false here, so fail immediately if
    // |check_error_state| is true. (For VPNs 'Connectable' is not reliable).
    NET_LOG(ERROR) << "Non VPN is unconfigured: " << service_path;
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
    NET_LOG_ERROR("No pending request to queue", service_path);
    return;
  }

  const int kMaxCertLoadTimeSeconds = 15;
  base::TimeDelta dtime = base::TimeTicks::Now() - logged_in_time_;
  if (dtime > base::TimeDelta::FromSeconds(kMaxCertLoadTimeSeconds)) {
    NET_LOG_ERROR("Certificate load timeout", service_path);
    InvokeConnectErrorCallback(service_path, request->error_callback,
                               kErrorCertLoadTimeout);
    return;
  }

  NET_LOG_EVENT("Connect Request Queued", service_path);
  queued_connect_.reset(
      new ConnectRequest(request->mode, service_path, request->profile_path,
                         request->success_callback, request->error_callback));
  pending_requests_.erase(service_path);

  // Post a delayed task to check to see if certificates have loaded. If they
  // haven't, and queued_connect_ has not been cleared (e.g. by a successful
  // connect request), cancel the request and notify the user.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NetworkConnectionHandlerImpl::CheckCertificatesLoaded,
                     AsWeakPtr()),
      base::TimeDelta::FromSeconds(kMaxCertLoadTimeSeconds) - dtime);
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
  NET_LOG_ERROR("Certificate load timeout", queued_connect_->service_path);
  InvokeConnectErrorCallback(queued_connect_->service_path,
                             queued_connect_->error_callback,
                             kErrorCertLoadTimeout);
  queued_connect_.reset();
  network_state_handler_->SetNetworkConnectRequested(
      queued_connect_->service_path, false);
}

void NetworkConnectionHandlerImpl::ConnectToQueuedNetwork() {
  DCHECK(queued_connect_);

  // Make a copy of |queued_connect_| parameters, because |queued_connect_|
  // will get reset at the beginning of |ConnectToNetwork|.
  std::string service_path = queued_connect_->service_path;
  base::Closure success_callback = queued_connect_->success_callback;
  network_handler::ErrorCallback error_callback =
      queued_connect_->error_callback;

  NET_LOG_EVENT("Connecting to Queued Network", service_path);
  ConnectToNetwork(service_path, success_callback, error_callback,
                   false /* check_error_state */, queued_connect_->mode);
}

void NetworkConnectionHandlerImpl::CallShillConnect(
    const std::string& service_path) {
  NET_LOG_EVENT("Sending Connect Request to Shill", service_path);
  network_state_handler_->ClearLastErrorForNetwork(service_path);
  ShillServiceClient::Get()->Connect(
      dbus::ObjectPath(service_path),
      base::Bind(&NetworkConnectionHandlerImpl::HandleShillConnectSuccess,
                 AsWeakPtr(), service_path),
      base::Bind(&NetworkConnectionHandlerImpl::HandleShillConnectFailure,
                 AsWeakPtr(), service_path));
}

void NetworkConnectionHandlerImpl::HandleConfigurationFailure(
    const std::string& service_path,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG(ERROR) << "Connect configuration failure: " << error_name;
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG_ERROR("HandleConfigurationFailure called with no pending request.",
                  service_path);
    return;
  }
  network_handler::ErrorCallback error_callback = request->error_callback;
  ClearPendingRequest(service_path);
  InvokeConnectErrorCallback(service_path, error_callback,
                             kErrorConfigureFailed);
}

void NetworkConnectionHandlerImpl::HandleShillConnectSuccess(
    const std::string& service_path) {
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG_ERROR("HandleShillConnectSuccess called with no pending request.",
                  service_path);
    return;
  }
  if (request->mode == ConnectCallbackMode::ON_STARTED) {
    if (!request->success_callback.is_null())
      request->success_callback.Run();
    // Request started; do not invoke success or error callbacks on
    // completion.
    request->success_callback = base::Closure();
    request->error_callback = network_handler::ErrorCallback();
  }
  request->connect_state = ConnectRequest::CONNECT_STARTED;
  NET_LOG_EVENT("Connect Request Acknowledged", service_path);
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
    NET_LOG_ERROR("HandleShillConnectFailure called with no pending request.",
                  service_path);
    return;
  }
  network_handler::ErrorCallback error_callback = request->error_callback;
  ClearPendingRequest(service_path);

  std::string error;
  if (dbus_error_name == shill::kErrorResultAlreadyConnected) {
    error = kErrorConnected;
  } else if (dbus_error_name == shill::kErrorResultInProgress) {
    error = kErrorConnecting;
  } else {
    error = kErrorConnectFailed;
  }
  NET_LOG(ERROR) << "Connect Failure: " << service_path << " Error: " << error
                 << " Shill error: " << dbus_error_name;
  InvokeConnectErrorCallback(service_path, error_callback, error);
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
    if (!request->profile_path.empty()) {
      // If a profile path was specified, set it on a successful connection.
      configuration_handler_->SetNetworkProfile(
          service_path, request->profile_path, base::DoNothing(),
          chromeos::network_handler::ErrorCallback());
    }
    InvokeConnectSuccessCallback(request->service_path,
                                 request->success_callback);
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
  if (connection_state == shill::kStateIdle && pending_requests_.size() > 1) {
    // Another connect request canceled this one.
    error_name = kErrorConnectCanceled;
  } else {
    error_name = kErrorConnectFailed;
    if (connection_state != shill::kStateFailure)
      NET_LOG_ERROR("Unexpected State: " + connection_state, service_path);
  }

  network_handler::ErrorCallback error_callback = request->error_callback;
  ClearPendingRequest(service_path);
  InvokeConnectErrorCallback(service_path, error_callback, error_name);
}

void NetworkConnectionHandlerImpl::CheckAllPendingRequests() {
  for (std::map<std::string, ConnectRequest>::iterator iter =
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
    NET_LOG_ERROR("ErrorCallbackForPendingRequest with no pending request.",
                  service_path);
    return;
  }
  // Remove the entry before invoking the callback in case it triggers a retry.
  network_handler::ErrorCallback error_callback = request->error_callback;
  ClearPendingRequest(service_path);

  InvokeConnectErrorCallback(service_path, error_callback, error_name);
}

// Disconnect

void NetworkConnectionHandlerImpl::CallShillDisconnect(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG_USER("Disconnect Request", service_path);
  ShillServiceClient::Get()->Disconnect(
      dbus::ObjectPath(service_path),
      base::Bind(&NetworkConnectionHandlerImpl::HandleShillDisconnectSuccess,
                 AsWeakPtr(), service_path, success_callback),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 kErrorDisconnectFailed, service_path, error_callback));
}

void NetworkConnectionHandlerImpl::HandleShillDisconnectSuccess(
    const std::string& service_path,
    const base::Closure& success_callback) {
  NET_LOG_EVENT("Disconnect Request Sent", service_path);
  if (!success_callback.is_null())
    success_callback.Run();
}

}  // namespace chromeos
