// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_ACTIVATION_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_ACTIVATION_HANDLER_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace chromeos {

// The NetworkActivationHandler class allows making service specific
// calls required for activation on mobile networks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkActivationHandler {
 public:
  NetworkActivationHandler(const NetworkActivationHandler&) = delete;
  NetworkActivationHandler& operator=(const NetworkActivationHandler&) = delete;

  virtual ~NetworkActivationHandler() = default;

  // CompleteActivation() will start an asynchronous activation completion
  // attempt.
  // On success, |success_callback| will be called.
  // On failure, |error_callback| will be called with |error_name| one of:
  //  kErrorNotFound if no network matching |service_path| is found.
  //  kErrorShillError if a DBus or Shill error occurred.
  virtual void CompleteActivation(
      const std::string& service_path,
      base::OnceClosure success_callback,
      network_handler::ErrorCallback error_callback) = 0;

 protected:
  NetworkActivationHandler() = default;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash {
using ::chromeos::NetworkActivationHandler;
}

#endif  // CHROMEOS_NETWORK_NETWORK_ACTIVATION_HANDLER_H_
