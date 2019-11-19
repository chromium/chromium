// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_IMPL_H_
#define CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_IMPL_H_

#include "base/component_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

// Implementation of NetworkConnectionHandler.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkConnectionHandlerImpl
    : public NetworkConnectionHandler,
      public LoginState::Observer,
      public NetworkCertLoader::Observer,
      public NetworkStateHandlerObserver,
      public base::SupportsWeakPtr<NetworkConnectionHandlerImpl> {
 public:
  NetworkConnectionHandlerImpl();
  ~NetworkConnectionHandlerImpl() override;

  // NetworkConnectionHandler:
  void ConnectToNetwork(const std::string& service_path,
                        const base::Closure& success_callback,
                        const network_handler::ErrorCallback& error_callback,
                        bool check_error_state,
                        ConnectCallbackMode mode) override;
  void DisconnectNetwork(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) override;

  // NetworkStateHandlerObserver
  void NetworkListChanged() override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;

  // LoginState::Observer
  void LoggedInStateChanged() override;

  // NetworkCertLoader::Observer
  void OnCertificatesLoaded() override;

  void Init(NetworkStateHandler* network_state_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler) override;

 private:
  struct ConnectRequest {
    ConnectRequest(ConnectCallbackMode mode,
                   const std::string& service_path,
                   const std::string& profile_path,
                   const base::Closure& success,
                   const network_handler::ErrorCallback& error);
    ~ConnectRequest();
    explicit ConnectRequest(const ConnectRequest& other);

    enum ConnectState {
      CONNECT_REQUESTED = 0,
      CONNECT_STARTED = 1,
      CONNECT_CONNECTING = 2
    };

    ConnectCallbackMode mode;
    std::string service_path;
    std::string profile_path;
    ConnectState connect_state;
    base::Closure success_callback;
    network_handler::ErrorCallback error_callback;
  };

  bool HasConnectingNetwork(const std::string& service_path);

  ConnectRequest* GetPendingRequest(const std::string& service_path);

  // Callback from Shill.Service.GetProperties. Parses |properties| to verify
  // whether or not the network appears to be configured. If configured,
  // attempts a connection, otherwise invokes error_callback from
  // pending_requests_[service_path]. |check_error_state| is passed from
  // ConnectToNetwork(), see comment for info.
  void VerifyConfiguredAndConnect(bool check_error_state,
                                  const std::string& service_path,
                                  const base::DictionaryValue& properties);

  // Queues a connect request until certificates have loaded.
  void QueueConnectRequest(const std::string& service_path);

  // Checks to see if certificates have loaded and if not, cancels any queued
  // connect request and notifies the user.
  void CheckCertificatesLoaded();

  // Handles connecting to a queued network after certificates are loaded or
  // handle cert load timeout.
  void ConnectToQueuedNetwork();

  // Calls Shill.Manager.Connect asynchronously.
  void CallShillConnect(const std::string& service_path);

  // Handles failure from ConfigurationHandler calls.
  void HandleConfigurationFailure(
      const std::string& service_path,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);

  // Handles success or failure from Shill.Service.Connect.
  void HandleShillConnectSuccess(const std::string& service_path);
  void HandleShillConnectFailure(const std::string& service_path,
                                 const std::string& error_name,
                                 const std::string& error_message);

  // Note: |service_path| is passed by value here, because in some cases
  // the value may be located in the map and then it can be deleted, producing
  // a reference to invalid memory.
  void CheckPendingRequest(const std::string service_path);

  void CheckAllPendingRequests();
  void ClearPendingRequest(const std::string& service_path);

  // Look up the ConnectRequest for |service_path| and call
  // InvokeConnectErrorCallback.
  void ErrorCallbackForPendingRequest(const std::string& service_path,
                                      const std::string& error_name);

  // Calls Shill.Manager.Disconnect asynchronously.
  void CallShillDisconnect(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback);

  // Handle success from Shill.Service.Disconnect.
  void HandleShillDisconnectSuccess(const std::string& service_path,
                                    const base::Closure& success_callback);

  // Local references to the associated handler instances.
  NetworkCertLoader* network_cert_loader_;
  NetworkStateHandler* network_state_handler_;
  NetworkConfigurationHandler* configuration_handler_;
  ManagedNetworkConfigurationHandler* managed_configuration_handler_;

  // Map of pending connect requests, used to prevent repeated attempts while
  // waiting for Shill and to trigger callbacks on eventual success or failure.
  std::map<std::string, ConnectRequest> pending_requests_;
  std::unique_ptr<ConnectRequest> queued_connect_;

  // Track certificate loading state.
  bool logged_in_;
  bool certificates_loaded_;
  base::TimeTicks logged_in_time_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_IMPL_H_
