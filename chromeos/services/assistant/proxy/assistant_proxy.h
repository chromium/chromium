// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_

#include <memory>

#include "base/threading/thread.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace assistant {

class ServiceController;

// The proxy to the Assistant service, which serves as the main
// access point to the entire Assistant API.
class AssistantProxy {
 public:
  AssistantProxy();
  AssistantProxy(AssistantProxy&) = delete;
  AssistantProxy& operator=(AssistantProxy&) = delete;
  ~AssistantProxy();

  // Returns the controller that manages starting and stopping of the Assistant
  // service.
  ServiceController& service_controller();

  // The background thread is temporary exposed until the entire Libassistant
  // API is hidden behind this proxy API.
  base::Thread& background_thread() { return background_thread_; }

 private:
  using LibassistantServiceMojom =
      chromeos::libassistant::mojom::LibassistantService;
  using ServiceControllerMojom =
      chromeos::libassistant::mojom::ServiceController;

  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner();

  void CreateMojomService();
  void CreateMojomServiceOnBackgroundThread(
      mojo::PendingReceiver<LibassistantServiceMojom>);
  void DestroyMojomService();
  void DestroyMojomServiceOnBackgroundThread();

  mojo::Remote<ServiceControllerMojom> BindServiceController();

  // The thread on which the Mojom service (and by extension Libassistant) runs.
  base::Thread background_thread_{"Assistant background thread"};

  mojo::Remote<LibassistantServiceMojom> client_;

  // The Mojom service that runs Libassistant.
  // For now this is locally owned but it will be moved to a sandbox later.
  std::unique_ptr<LibassistantServiceMojom> mojom_service_;

  std::unique_ptr<ServiceController> service_controller_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_
