// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace chromeos {

// The NetworkConnectionHandler class is used to manage network connection
// requests. This is the only class that should make Shill Connect calls.
// It handles the following steps:
// 1. Determine whether or not sufficient information (e.g. passphrase) is
//    known to be available to connect to the network.
// 2. Request additional information (e.g. user data which contains certificate
//    information) and determine whether sufficient information is available.
// 3. Possibly configure the network certificate info (tpm slot and pkcs11 id).
// 4. Send the connect request.
// 5. Wait for the network state to change to a non connecting state.
// 6. Invoke the appropriate callback (always) on success or failure.
//
// NetworkConnectionHandler depends on NetworkStateHandler for immediately
// available State information, and NetworkConfigurationHandler for any
// configuration calls.

enum class ConnectCallbackMode { ON_STARTED, ON_COMPLETED };

class NetworkStateHandler;
class NetworkConfigurationHandler;
class ManagedNetworkConfigurationHandler;

class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkConnectionHandler {
 public:
  // Constants for |error_name| from |error_callback| for Connect.

  //  No network matching |service_path| is found (hidden networks must be
  //  configured before connecting).
  static const char kErrorNotFound[];

  // Already connected to the network.
  static const char kErrorConnected[];

  // Already connecting to the network.
  static const char kErrorConnecting[];

  // The passphrase is missing or invalid.
  static const char kErrorPassphraseRequired[];

  // The passphrase is incorrect.
  static const char kErrorBadPassphrase[];

  // The network requires a cert and none exists.
  static const char kErrorCertificateRequired[];

  // The network had an authentication error, indicating that additional or
  // different authentication information is required.
  static const char kErrorAuthenticationRequired[];

  // Additional configuration is required.
  static const char kErrorConfigurationRequired[];

  // Configuration failed during the configure stage of the connect flow.
  static const char kErrorConfigureFailed[];

  // An unexpected DBus, Shill, or Tether communication error occurred while
  // connecting.
  static const char kErrorConnectFailed[];

  // An unexpected DBus, Shill, or Tether error occurred while disconnecting.
  static const char kErrorDisconnectFailed[];

  // A new network connect request canceled this one.
  static const char kErrorConnectCanceled[];

  // Constants for |error_name| from |error_callback| for Disconnect.
  static const char kErrorNotConnected[];

  // Certificate load timed out.
  static const char kErrorCertLoadTimeout[];

  // Trying to configure a network that is blocked by policy.
  static const char kErrorBlockedByPolicy[];

  // The HexSSID is missing.
  static const char kErrorHexSsidRequired[];

  // Network activation failed.
  static const char kErrorActivateFailed[];

  // Network was enabled/disabled when it was not available.
  static const char kErrorEnabledOrDisabledWhenNotAvailable[];

  // Connection or disconnection to Tether network attempted when no tether
  // delegate present.
  static const char kErrorTetherAttemptWithNoDelegate[];

  class COMPONENT_EXPORT(CHROMEOS_NETWORK) TetherDelegate {
   public:
    // Connects to the Tether network with GUID |tether_network_guid|. On
    // success, invokes |success_callback|, and on failure, invokes
    // |error_callback|, passing the relevant error code declared above.
    virtual void ConnectToNetwork(
        const std::string& tether_network_guid,
        const base::Closure& success_callback,
        const network_handler::StringResultCallback& error_callback) = 0;

    // Disconnects from the Tether network with GUID |tether_network_guid|. On
    // success, invokes |success_callback|, and on failure, invokes
    // |error_callback|, passing the relevant error code declared above.
    virtual void DisconnectFromNetwork(
        const std::string& tether_network_guid,
        const base::Closure& success_callback,
        const network_handler::StringResultCallback& error_callback) = 0;

   protected:
    virtual ~TetherDelegate() {}
  };

  virtual ~NetworkConnectionHandler();

  void AddObserver(NetworkConnectionObserver* observer);
  void RemoveObserver(NetworkConnectionObserver* observer);

  // Sets the TetherDelegate to handle Tether actions. |tether_delegate| is
  // owned by the caller.
  void SetTetherDelegate(TetherDelegate* tether_delegate);

  // ConnectToNetwork() will start an asynchronous connection attempt.
  // |success_callback| will be called if the connection request succeeds
  //   or if a request is sent if |mode| is ON_STARTED (see below).
  // |error_callback| will be called with |error_name| set to one of the
  //   constants defined above if the connection request fails.
  // |error_message| will contain an additional error string for debugging.
  // If |check_error_state| is true, the current state of the network is
  //   checked for errors, otherwise current state is ignored (e.g. for recently
  //   configured networks or repeat attempts).
  // If |mode| is ON_STARTED, |success_callback| will be invoked when the
  //   connect request is successfully made and not when the connection
  //   completes. Note: This also prevents |error_callback| from being called
  //   if the connection request is successfully sent but the network does not
  //   connect.
  virtual void ConnectToNetwork(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback,
      bool check_error_state,
      ConnectCallbackMode mode) = 0;

  // DisconnectNetwork() will send a Disconnect request to Shill.
  // On success, |success_callback| will be called.
  // On failure, |error_callback| will be called with |error_name| one of:
  //  kErrorNotFound if no network matching |service_path| is found.
  //  kErrorNotConnected if not connected to the network.
  //  kErrorDisconnectFailed if a DBus or Shill error occurred.
  // |error_message| will contain and additional error string for debugging.
  virtual void DisconnectNetwork(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  virtual void Init(NetworkStateHandler* network_state_handler,
                    NetworkConfigurationHandler* network_configuration_handler,
                    ManagedNetworkConfigurationHandler*
                        managed_network_configuration_handler) = 0;

  // Construct and initialize an instance for testing.
  static std::unique_ptr<NetworkConnectionHandler> InitializeForTesting(
      NetworkStateHandler* network_state_handler,
      NetworkConfigurationHandler* network_configuration_handler,
      ManagedNetworkConfigurationHandler*
          managed_network_configuration_handler);

 protected:
  NetworkConnectionHandler();

  // Notify caller and observers that the connect request succeeded.
  void InvokeConnectSuccessCallback(const std::string& service_path,
                                    const base::Closure& success_callback);

  // Notify caller and observers that the connect request failed.
  // |error_name| will be one of the kError* messages defined above.
  void InvokeConnectErrorCallback(
      const std::string& service_path,
      const network_handler::ErrorCallback& error_callback,
      const std::string& error_name);

  // Initiates a connection to a Tether network.
  void InitiateTetherNetworkConnection(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback);

  // Initiates a disconnection from a Tether network.
  void InitiateTetherNetworkDisconnection(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback);

  base::ObserverList<NetworkConnectionObserver, true>::Unchecked observers_;

  // Delegate used to start a connection to a tether network.
  TetherDelegate* tether_delegate_;

 private:
  // Only to be used by NetworkConnectionHandler implementation (and not by
  // derived classes).
  base::WeakPtrFactory<NetworkConnectionHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CONNECTION_HANDLER_H_
