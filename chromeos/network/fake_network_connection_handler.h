// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_FAKE_NETWORK_CONNECTION_HANDLER_H_
#define CHROMEOS_NETWORK_FAKE_NETWORK_CONNECTION_HANDLER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace chromeos {

// Fake NetworkConnectionHandler implementation for tests.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) FakeNetworkConnectionHandler
    : public NetworkConnectionHandler {
 public:
  FakeNetworkConnectionHandler();
  ~FakeNetworkConnectionHandler() override;

  // Parameters captured by calls to ConnectToNetwork() and DisconnectNetwork().
  // Accessible to clients via connect_calls() and disconnect_calls().
  class ConnectionParams {
   public:
    // For ConnectToNetwork() calls.
    ConnectionParams(const std::string& service_path,
                     const base::Closure& success_callback,
                     const network_handler::ErrorCallback& error_callback,
                     bool check_error_state,
                     ConnectCallbackMode connect_callback_mode);

    // For DisconnectNetwork() calls.
    ConnectionParams(const std::string& service_path,
                     const base::Closure& success_callback,
                     const network_handler::ErrorCallback& error_callback);

    ConnectionParams(const ConnectionParams& other);
    ~ConnectionParams();

    const std::string& service_path() const { return service_path_; }

    // check_error_state() and connect_callback_mode() should only be called for
    // ConnectionParams objects corresponding to ConnectToNetwork() calls.
    bool check_error_state() const { return *check_error_state_; }
    ConnectCallbackMode connect_callback_mode() const {
      return *connect_callback_mode_;
    }

    void InvokeSuccessCallback() const;
    void InvokeErrorCallback(
        const std::string& error_name,
        std::unique_ptr<base::DictionaryValue> error_data) const;

   private:
    std::string service_path_;
    base::Closure success_callback_;
    network_handler::ErrorCallback error_callback_;
    base::Optional<bool> check_error_state_;
    base::Optional<ConnectCallbackMode> connect_callback_mode_;
  };

  const std::vector<ConnectionParams>& connect_calls() const {
    return connect_calls_;
  }
  const std::vector<ConnectionParams>& disconnect_calls() const {
    return disconnect_calls_;
  }

 private:
  // NetworkConnectionHandler:
  void ConnectToNetwork(const std::string& service_path,
                        const base::Closure& success_callback,
                        const network_handler::ErrorCallback& error_callback,
                        bool check_error_state,
                        ConnectCallbackMode connect_callback_mode) override;
  void DisconnectNetwork(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) override;
  void Init(NetworkStateHandler* network_state_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler) override;

  std::vector<ConnectionParams> connect_calls_;
  std::vector<ConnectionParams> disconnect_calls_;

  DISALLOW_COPY_AND_ASSIGN(FakeNetworkConnectionHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_FAKE_NETWORK_CONNECTION_HANDLER_H_
