// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_ACTIVATION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_ACTIVATION_HANDLER_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/ash/components/network/network_activation_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"

namespace ash {

// Fake NetworkActivationHandler implementation for tests.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) FakeNetworkActivationHandler
    : public NetworkActivationHandler {
 public:
  FakeNetworkActivationHandler();

  FakeNetworkActivationHandler(const FakeNetworkActivationHandler&) = delete;
  FakeNetworkActivationHandler& operator=(const FakeNetworkActivationHandler&) =
      delete;

  ~FakeNetworkActivationHandler() override;

  // Parameters captured by calls to CompleteActivation().
  // Accessible to clients via complete_activation_calls().
  class ActivationParams {
   public:
    ActivationParams(const std::string& service_path,
                     base::OnceClosure success_callback,
                     network_handler::ErrorCallback error_callback);

    ActivationParams(ActivationParams&& other);
    ~ActivationParams();

    const std::string& service_path() const { return service_path_; }

    void InvokeSuccessCallback();
    void InvokeErrorCallback(const std::string& error_name);

   private:
    std::string service_path_;
    base::OnceClosure success_callback_;
    network_handler::ErrorCallback error_callback_;
  };

  std::vector<ActivationParams>& complete_activation_calls() {
    return complete_activation_calls_;
  }

 private:
  // NetworkActivationHandler:
  void CompleteActivation(
      const std::string& service_path,
      base::OnceClosure success_callback,
      network_handler::ErrorCallback error_callback) override;

  std::vector<ActivationParams> complete_activation_calls_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_ACTIVATION_HANDLER_H_
