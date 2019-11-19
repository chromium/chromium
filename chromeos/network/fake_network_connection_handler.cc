// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/fake_network_connection_handler.h"

namespace chromeos {

FakeNetworkConnectionHandler::ConnectionParams::ConnectionParams(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback,
    bool check_error_state,
    ConnectCallbackMode connect_callback_mode)
    : service_path_(service_path),
      success_callback_(success_callback),
      error_callback_(error_callback),
      check_error_state_(check_error_state),
      connect_callback_mode_(connect_callback_mode) {}

FakeNetworkConnectionHandler::ConnectionParams::ConnectionParams(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback)
    : service_path_(service_path),
      success_callback_(success_callback),
      error_callback_(error_callback) {}

FakeNetworkConnectionHandler::ConnectionParams::ConnectionParams(
    const ConnectionParams& other) = default;

FakeNetworkConnectionHandler::ConnectionParams::~ConnectionParams() = default;

void FakeNetworkConnectionHandler::ConnectionParams::InvokeSuccessCallback()
    const {
  success_callback_.Run();
}

void FakeNetworkConnectionHandler::ConnectionParams::InvokeErrorCallback(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) const {
  error_callback_.Run(error_name, std::move(error_data));
}

FakeNetworkConnectionHandler::FakeNetworkConnectionHandler() = default;

FakeNetworkConnectionHandler::~FakeNetworkConnectionHandler() = default;

void FakeNetworkConnectionHandler::ConnectToNetwork(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback,
    bool check_error_state,
    ConnectCallbackMode connect_callback_mode) {
  connect_calls_.emplace_back(service_path, success_callback, error_callback,
                              check_error_state, connect_callback_mode);
}

void FakeNetworkConnectionHandler::DisconnectNetwork(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  disconnect_calls_.emplace_back(service_path, success_callback,
                                 error_callback);
}

void FakeNetworkConnectionHandler::Init(
    NetworkStateHandler* network_state_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  // No initialization necessary for a test double.
}

}  // namespace chromeos
