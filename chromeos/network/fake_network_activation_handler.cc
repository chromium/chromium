// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/fake_network_activation_handler.h"

#include "base/callback.h"
#include "base/values.h"

namespace chromeos {

FakeNetworkActivationHandler::ActivationParams::ActivationParams(
    const std::string& service_path,
    const std::string& carrier,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback)
    : service_path_(service_path),
      carrier_(carrier),
      success_callback_(success_callback),
      error_callback_(error_callback) {}

FakeNetworkActivationHandler::ActivationParams::ActivationParams(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback)
    : service_path_(service_path),
      success_callback_(success_callback),
      error_callback_(error_callback) {}

FakeNetworkActivationHandler::ActivationParams::ActivationParams(
    const ActivationParams& other) = default;

FakeNetworkActivationHandler::ActivationParams::~ActivationParams() = default;

void FakeNetworkActivationHandler::ActivationParams::InvokeSuccessCallback()
    const {
  success_callback_.Run();
}

void FakeNetworkActivationHandler::ActivationParams::InvokeErrorCallback(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) const {
  error_callback_.Run(error_name, std::move(error_data));
}

FakeNetworkActivationHandler::FakeNetworkActivationHandler() = default;

FakeNetworkActivationHandler::~FakeNetworkActivationHandler() = default;

void FakeNetworkActivationHandler::Activate(
    const std::string& service_path,
    const std::string& carrier,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  activate_calls_.emplace_back(service_path, carrier, success_callback,
                               error_callback);
}

void FakeNetworkActivationHandler::CompleteActivation(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  complete_activation_calls_.emplace_back(service_path, success_callback,
                                          error_callback);
}

}  // namespace chromeos
