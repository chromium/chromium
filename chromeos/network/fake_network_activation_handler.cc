// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/fake_network_activation_handler.h"

#include "base/callback.h"
#include "base/values.h"

namespace chromeos {

FakeNetworkActivationHandler::ActivationParams::ActivationParams(
    const std::string& service_path,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback)
    : service_path_(service_path),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)) {}

FakeNetworkActivationHandler::ActivationParams::ActivationParams(
    ActivationParams&& other) = default;

FakeNetworkActivationHandler::ActivationParams::~ActivationParams() = default;

void FakeNetworkActivationHandler::ActivationParams::InvokeSuccessCallback() {
  std::move(success_callback_).Run();
}

void FakeNetworkActivationHandler::ActivationParams::InvokeErrorCallback(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  std::move(error_callback_).Run(error_name, std::move(error_data));
}

FakeNetworkActivationHandler::FakeNetworkActivationHandler() = default;

FakeNetworkActivationHandler::~FakeNetworkActivationHandler() = default;

void FakeNetworkActivationHandler::CompleteActivation(
    const std::string& service_path,
    base::OnceClosure success_callback,
    network_handler::ErrorCallback error_callback) {
  complete_activation_calls_.emplace_back(
      service_path, std::move(success_callback), std::move(error_callback));
}

}  // namespace chromeos
