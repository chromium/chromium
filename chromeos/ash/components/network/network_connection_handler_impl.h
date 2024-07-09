// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONNECTION_HANDLER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONNECTION_HANDLER_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace ash {

// Implementation of NetworkConnectionHandler.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkConnectionHandlerImpl
    : public NetworkConnectionHandler,
      public NetworkCertLoader::Observer,
      public NetworkStateHandlerObserver {
 public:
  NetworkConnectionHandlerImpl();

  NetworkConnectionHandlerImpl(const NetworkConnectionHandlerImpl&) = delete;
  NetworkConnectionHandlerImpl& operator=(const NetworkConnectionHandlerImpl&) =
      delete;

  ~NetworkConnectionHandlerImpl() override;

  // NetworkConnectionHandler:
  void ConnectToNetwork(const std::string& service_path,
                        base::OnceClosure success_callback,
                        network_handler::ErrorCallback error_callback,
                        bool check_error_state,
                        ConnectCallbackMode mode) override;
  void DisconnectNetwork(
      const std::string& service_path,
      base::OnceClosure success_callback,
      network_handler::ErrorCallback error_callback) override;
  void OnAutoConnectedInitiated(int auto_connect_reasons) override;

  // NetworkStateHandlerObserver
  void NetworkListChanged() override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;
  void NetworkIdentifierTransitioned(const std::string& old_service_path,
                                     const std::string& new_service_path,
                                     const std::string& old_guid,
                                     const std::string& new_guid) override;

  // NetworkCertLoader::Observer
  void OnCertificatesLoaded() override;

  void Init(
      NetworkStateHandler* network_state_handler,
      NetworkConfigurationHandler* network_configuration_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      CellularConnectionHandler* cellular_connection_handler) override;

 private:
  // Cellular configuration failure type. These values are persisted to logs.
  // Entries should not be renumbered and numeric values should
  // never be reused.
  enum class CellularConfigurationFailureType {
    kFailureGetShillProperties = 0,
    kFailurePropertiesWithNoType = 1,
    kFailureSetShillProperties = 2,
    kMaxValue = kFailureSetShillProperties
  };

  struct ConnectRequest {
    ConnectRequest(ConnectCallbackMode mode,
                   const std::string& service_path,
                   const std::string& profile_path,
                   base::OnceClosure success_callback,
                   network_handler::ErrorCallback error);
    ~ConnectRequest();
    ConnectRequest(ConnectRequest&&);

    enum ConnectState {
      CONNECT_REQUESTED = 0,
      CONNECT_STARTED = 1,
      CONNECT_CONNECTING = 2
    };

    ConnectCallbackMode mode;
    std::string service_path;
    std::string profile_path;
    ConnectState connect_state;
    base::OnceClosure success_callback;
    network_handler::ErrorCallback error_callback;
    std::unique_ptr<base::OneShotTimer> timer;
  };

  bool HasConnectingNetwork(const std::string& service_path);

  ConnectRequest* GetPendingRequest(const std::string& service_path);
  bool HasPendingCellularRequest() const;

  // Callback when PrepareExistingCellularNetworkForConnection succeeded.
  void OnPrepareCellularNetworkForConnectionSuccess(
      const std::string& service_path,
      bool auto_connected);

  // Callback when PrepareExistingCellularNetworkForConnection failed.
  void OnPrepareCellularNetworkForConnectionFailure(
      const std::string& service_path,
      const std::string& error_name);

  void StartConnectTimer(const std::string& service_path,
                         base::TimeDelta timeout);
  void OnConnectTimeout(ConnectRequest* service_path);

  // Callback from Shill.Service.GetProperties. Parses |properties| to verify
  // whether or not the network appears to be configured. If configured,
  // attempts a connection, otherwise invokes error_callback from
  // pending_requests_[service_path]. |check_error_state| is passed from
  // ConnectToNetwork(), see comment for info.
  void VerifyConfiguredAndConnect(bool check_error_state,
                                  const std::string& service_path,
                                  std::optional<base::Value::Dict> properties);

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

  // Called when setting shill properties fails.
  void OnSetShillPropertiesFailed(const std::string& service_path,
                                  const std::string& error_name);

  // Handles failure from ConfigurationHandler calls.
  void HandleConfigurationFailure(
      const std::string& service_path,
      const std::string& error_name,
      CellularConfigurationFailureType failure_type);

  // Handles success or failure from Shill.Service.Connect.
  void HandleShillConnectSuccess(const std::string& service_path);
  void HandleShillConnectFailure(const std::string& service_path,
                                 const std::string& error_name,
                                 const std::string& error_message);

  // Sets connection request to started and calls callback if necessary.
  void HandleNetworkConnectStarted(ConnectRequest* request);

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
  void CallShillDisconnect(const std::string& service_path,
                           base::OnceClosure success_callback,
                           network_handler::ErrorCallback error_callback);

  // Handle success from Shill.Service.Disconnect.
  void HandleShillDisconnectSuccess(const std::string& service_path,
                                    base::OnceClosure success_callback);

  // Logs the cellular configuration failure type to UMA.
  void LogConfigurationFailureTypeIfCellular(
      const std::string& service_path,
      CellularConfigurationFailureType failure_type);

  // Local references to the associated handler instances.
  raw_ptr<NetworkCertLoader> network_cert_loader_ = nullptr;
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  raw_ptr<NetworkConfigurationHandler> configuration_handler_ = nullptr;
  raw_ptr<ManagedNetworkConfigurationHandler, DanglingUntriaged>
      managed_configuration_handler_ = nullptr;
  raw_ptr<CellularConnectionHandler> cellular_connection_handler_ = nullptr;

  // Map of pending connect requests, used to prevent repeated attempts while
  // waiting for Shill and to trigger callbacks on eventual success or failure.
  std::map<std::string, std::unique_ptr<ConnectRequest>> pending_requests_;
  std::unique_ptr<ConnectRequest> queued_connect_;

  // Track certificate loading state.
  bool certificates_loaded_ = false;
  // Track if there's a connection triggered by policy auto-connect.
  bool has_policy_auto_connect_ = false;

  base::WeakPtrFactory<NetworkConnectionHandlerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONNECTION_HANDLER_IMPL_H_
