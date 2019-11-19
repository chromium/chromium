// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_ACTIVATION_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_ACTIVATION_HANDLER_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace chromeos {

// The NetworkActivationHandler class allows making service specific
// calls required for activation on mobile networks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkActivationHandler {
 public:
  virtual ~NetworkActivationHandler() = default;

  // ActivateNetwork() will start an asynchronous activation attempt.
  // |carrier| may be empty or may specify a carrier to activate.
  // On success, |success_callback| will be called.
  // On failure, |error_callback| will be called with |error_name| one of:
  //  kErrorNotFound if no network matching |service_path| is found.
  //  kErrorShillError if a DBus or Shill error occurred.
  virtual void Activate(
      const std::string& service_path,
      const std::string& carrier,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // CompleteActivation() will start an asynchronous activation completion
  // attempt.
  // On success, |success_callback| will be called.
  // On failure, |error_callback| will be called with |error_name| one of:
  //  kErrorNotFound if no network matching |service_path| is found.
  //  kErrorShillError if a DBus or Shill error occurred.
  virtual void CompleteActivation(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) = 0;

 protected:
  NetworkActivationHandler() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkActivationHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_ACTIVATION_HANDLER_H_
