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
namespace libassistant {
class LibassistantService;
}  // namespace libassistant
}  // namespace chromeos

namespace chromeos {
namespace assistant {

class LibassistantServiceHost;
class ServiceControllerProxy;

// The proxy to the Assistant service, which serves as the main
// access point to the entire Assistant API.
class AssistantProxy {
 public:
  AssistantProxy();
  AssistantProxy(AssistantProxy&) = delete;
  AssistantProxy& operator=(AssistantProxy&) = delete;
  ~AssistantProxy();

  void Initialize(LibassistantServiceHost* host);

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

  void LaunchLibassistantService();
  void LaunchLibassistantServiceOnBackgroundThread(
      mojo::PendingReceiver<LibassistantServiceMojom>);
  void StopLibassistantService();
  void StopLibassistantServiceOnBackgroundThread();

  mojo::PendingRemote<ServiceControllerMojom> BindServiceController();

  // Owned by |AssistantManagerServiceImpl|.
  LibassistantServiceHost* libassistant_service_host_ = nullptr;
  mojo::Remote<LibassistantServiceMojom> libassistant_service_remote_;

  std::unique_ptr<ServiceControllerProxy> service_controller_proxy_;

  // The thread on which the Libassistant service runs.
  // Warning: must be the last object, so it is destroyed (and flushed) first.
  // This will prevent use-after-free issues where the background thread would
  // access other member variables after they have been destroyed.
  base::Thread background_thread_{"Assistant background thread"};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_ASSISTANT_PROXY_H_
