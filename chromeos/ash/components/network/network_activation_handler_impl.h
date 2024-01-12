// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_ACTIVATION_HANDLER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_ACTIVATION_HANDLER_IMPL_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/network_activation_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"

namespace ash {

// The NetworkActivationHandlerImpl class allows making service specific
// calls required for activation on mobile networks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkActivationHandlerImpl
    : public NetworkActivationHandler {
 public:
  NetworkActivationHandlerImpl(const NetworkActivationHandlerImpl&) = delete;
  NetworkActivationHandlerImpl& operator=(const NetworkActivationHandlerImpl&) =
      delete;

  ~NetworkActivationHandlerImpl() override;

 private:
  // NetworkActivationHandler:
  void CompleteActivation(
      const std::string& service_path,
      base::OnceClosure success_callback,
      network_handler::ErrorCallback error_callback) override;

 private:
  friend class NetworkHandler;

  NetworkActivationHandlerImpl();

  void HandleShillSuccess(base::OnceClosure success_callback);

  base::WeakPtrFactory<NetworkActivationHandlerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_ACTIVATION_HANDLER_IMPL_H_
