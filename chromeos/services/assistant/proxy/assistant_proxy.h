// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_

#include <memory>

#include "base/threading/thread.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace assistant_client {
class PlatformApi;
}  // namespace assistant_client

namespace chromeos {
namespace libassistant {
class LibassistantService;
}  // namespace libassistant
}  // namespace chromeos

namespace chromeos {
namespace assistant {

class AssistantManagerServiceDelegate;
class ServiceControllerProxy;

// The proxy to the Assistant service, which serves as the main
// access point to the entire Assistant API.
class AssistantProxy {
 public:
  AssistantProxy();
  AssistantProxy(AssistantProxy&) = delete;
  AssistantProxy& operator=(AssistantProxy&) = delete;
  ~AssistantProxy();

  void Initialize(assistant_client::PlatformApi* platform_api,
                  AssistantManagerServiceDelegate* delegate);

  // Returns the controller that manages starting and stopping of the Assistant
  // service.
  ServiceControllerProxy& service_controller();

  // The background thread is temporary exposed until the entire Libassistant
  // API is hidden behind this proxy API.
  base::Thread& background_thread() { return background_thread_; }

 private:
  using LibassistantServiceMojom =
      chromeos::libassistant::mojom::LibassistantService;
  using ServiceControllerMojom =
      chromeos::libassistant::mojom::ServiceController;

  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner();

  void CreateLibassistantService(assistant_client::PlatformApi* platform_api,
                                 AssistantManagerServiceDelegate* delegate);
  void CreateLibassistantServiceOnBackgroundThread(
      mojo::PendingReceiver<LibassistantServiceMojom>,
      assistant_client::PlatformApi* platform_api,
      AssistantManagerServiceDelegate* delegate);
  void DestroyLibassistantService();
  void DestroyLibassistantServiceOnBackgroundThread();

  mojo::PendingRemote<ServiceControllerMojom> BindServiceController();

  // The thread on which the Libassistant service.
  base::Thread background_thread_{"Assistant background thread"};

  mojo::Remote<LibassistantServiceMojom> libassistant_service_remote_;

  // The Mojom service that runs Libassistant.
  // For now this is locally owned but it will be moved to a sandbox later.
  std::unique_ptr<chromeos::libassistant::LibassistantService>
      libassistant_service_;

  std::unique_ptr<ServiceControllerProxy> service_controller_proxy_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_
