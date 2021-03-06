// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_PROXY_H_

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace network {
class PendingSharedURLLoaderFactory;
class SharedURLLoaderFactory;
}  // namespace network

namespace chromeos {
namespace assistant {

// Component managing the lifecycle of Libassistant,
// exposing methods to start/stop and configure Libassistant.
class ServiceControllerProxy {
 public:
  ServiceControllerProxy(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      mojo::PendingRemote<chromeos::libassistant::mojom::ServiceController>
          client);

  ServiceControllerProxy(ServiceControllerProxy&) = delete;
  ServiceControllerProxy& operator=(ServiceControllerProxy&) = delete;
  ~ServiceControllerProxy();

  // Initialize the |AssistantManager| and all related objects.
  //
  // Start() can only be called when the service is stopped.
  void Start(
      chromeos::libassistant::mojom::BootupConfigPtr bootup_config);
  // Stop and destroy the |AssistantManager| and all related objects.
  void Stop();
  void ResetAllDataAndStop();

  void AddAndFireStateObserver(
      mojo::PendingRemote<chromeos::libassistant::mojom::StateObserver>
          observer);

 private:
  mojo::PendingRemote<network::mojom::URLLoaderFactory> BindURLLoaderFactory();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  mojo::Remote<chromeos::libassistant::mojom::ServiceController>
      service_controller_remote_;

  base::WeakPtrFactory<ServiceControllerProxy> weak_factory_{this};
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_SERVICE_CONTROLLER_PROXY_H_
