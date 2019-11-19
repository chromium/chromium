// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_FAKE_NETWORK_ACTIVATION_HANDLER_H_
#define CHROMEOS_NETWORK_FAKE_NETWORK_ACTIVATION_HANDLER_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/network/network_activation_handler.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace chromeos {

// Fake NetworkActivationHandler implementation for tests.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) FakeNetworkActivationHandler
    : public NetworkActivationHandler {
 public:
  FakeNetworkActivationHandler();
  ~FakeNetworkActivationHandler() override;

  // Parameters captured by calls to Activate() and CompleteActivation().
  // Accessible to clients via activate_calls() and complete_activation_calls().
  class ActivationParams {
   public:
    // For Activate() calls.
    ActivationParams(const std::string& service_path,
                     const std::string& carrier,
                     const base::Closure& success_callback,
                     const network_handler::ErrorCallback& error_callback);

    // For CompleteActivation() calls.
    ActivationParams(const std::string& service_path,
                     const base::Closure& success_callback,
                     const network_handler::ErrorCallback& error_callback);

    ActivationParams(const ActivationParams& other);
    ~ActivationParams();

    const std::string& service_path() const { return service_path_; }

    // Should only be called on ActivationParams objects corresponding to
    // Activate() calls.
    const std::string& carrier() const { return *carrier_; }

    void InvokeSuccessCallback() const;
    void InvokeErrorCallback(
        const std::string& error_name,
        std::unique_ptr<base::DictionaryValue> error_data) const;

   private:
    std::string service_path_;
    base::Optional<std::string> carrier_;
    base::Closure success_callback_;
    network_handler::ErrorCallback error_callback_;
  };

  const std::vector<ActivationParams>& activate_calls() const {
    return activate_calls_;
  }
  const std::vector<ActivationParams>& complete_activation_calls() const {
    return complete_activation_calls_;
  }

 private:
  // NetworkActivationHandler:
  void Activate(const std::string& service_path,
                const std::string& carrier,
                const base::Closure& success_callback,
                const network_handler::ErrorCallback& error_callback) override;
  void CompleteActivation(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) override;

  std::vector<ActivationParams> activate_calls_;
  std::vector<ActivationParams> complete_activation_calls_;

  DISALLOW_COPY_AND_ASSIGN(FakeNetworkActivationHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_FAKE_NETWORK_ACTIVATION_HANDLER_H_
