// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_CONNECTION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_CONNECTION_HANDLER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"

namespace ash {

// Fake NetworkConnectionHandler implementation for tests.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) FakeNetworkConnectionHandler
    : public NetworkConnectionHandler {
 public:
  FakeNetworkConnectionHandler();

  FakeNetworkConnectionHandler(const FakeNetworkConnectionHandler&) = delete;
  FakeNetworkConnectionHandler& operator=(const FakeNetworkConnectionHandler&) =
      delete;

  ~FakeNetworkConnectionHandler() override;

  // Parameters captured by calls to ConnectToNetwork() and DisconnectNetwork().
  // Accessible to clients via connect_calls() and disconnect_calls().
  class ConnectionParams {
   public:
    // For ConnectToNetwork() calls.
    ConnectionParams(const std::string& service_path,
                     base::OnceClosure success_callback,
                     network_handler::ErrorCallback error_callback,
                     bool check_error_state,
                     ConnectCallbackMode connect_callback_mode);

    // For DisconnectNetwork() calls.
    ConnectionParams(const std::string& service_path,
                     base::OnceClosure success_callback,
                     network_handler::ErrorCallback error_callback);

    ConnectionParams(ConnectionParams&&);
    ~ConnectionParams();

    const std::string& service_path() const { return service_path_; }

    // check_error_state() and connect_callback_mode() should only be called for
    // ConnectionParams objects corresponding to ConnectToNetwork() calls.
    bool check_error_state() const { return *check_error_state_; }
    ConnectCallbackMode connect_callback_mode() const {
      return *connect_callback_mode_;
    }

    void InvokeSuccessCallback();
    void InvokeErrorCallback(const std::string& error_name);

   private:
    std::string service_path_;
    base::OnceClosure success_callback_;
    network_handler::ErrorCallback error_callback_;
    std::optional<bool> check_error_state_;
    std::optional<ConnectCallbackMode> connect_callback_mode_;
  };

  std::vector<ConnectionParams>& connect_calls() { return connect_calls_; }
  std::vector<ConnectionParams>& disconnect_calls() {
    return disconnect_calls_;
  }

 private:
  // NetworkConnectionHandler:
  void ConnectToNetwork(const std::string& service_path,
                        base::OnceClosure success_callback,
                        network_handler::ErrorCallback error_callback,
                        bool check_error_state,
                        ConnectCallbackMode connect_callback_mode) override;
  void DisconnectNetwork(
      const std::string& service_path,
      base::OnceClosure success_callback,
      network_handler::ErrorCallback error_callback) override;
  void Init(
      NetworkStateHandler* network_state_handler,
      NetworkConfigurationHandler* network_configuration_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      CellularConnectionHandler* cellular_connection_handler) override;
  void OnAutoConnectedInitiated(int reason) override {}

  std::vector<ConnectionParams> connect_calls_;
  std::vector<ConnectionParams> disconnect_calls_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_CONNECTION_HANDLER_H_
