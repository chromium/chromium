// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_SERVICE_CONTROLLER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_SERVICE_CONTROLLER_H_

#include "base/component_export.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom-shared.h"
#include "libassistant/shared/public/assistant_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
class PlatformApi;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {
class LibassistantV1Api;
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace assistant {
class AssistantManagerServiceDelegate;
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

// Component managing the lifecycle of Libassistant,
// exposing methods to start/stop and configure Libassistant.
// Note: to access the Libassistant objects from //chromeos/services/assistant,
// use the |LibassistantV1Api| singleton, which will be populated by this class.
class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) ServiceController
    : public mojom::ServiceController {
 public:
  ServiceController(assistant::AssistantManagerServiceDelegate* delegate,
                    assistant_client::PlatformApi* platform_api);
  ServiceController(ServiceController&) = delete;
  ServiceController& operator=(ServiceController&) = delete;
  ~ServiceController() override;

  void Bind(mojo::PendingReceiver<mojom::ServiceController> receiver);

  // mojom::ServiceController implementation:
  void Start(const std::string& libassistant_config) override;
  void Stop() override;
  void AddAndFireStateObserver(
      mojo::PendingRemote<mojom::StateObserver> observer) override;

  // Will return nullptr if the service is stopped.
  assistant_client::AssistantManager* assistant_manager();
  // Will return nullptr if the service is stopped.
  assistant_client::AssistantManagerInternal* assistant_manager_internal();

 private:
  void SetStateAndInformObservers(mojom::ServiceState new_state);

  mojom::ServiceState state_ = mojom::ServiceState::kStopped;

  // Owned by |AssistantManagerServiceImpl| which indirectly owns us.
  assistant::AssistantManagerServiceDelegate* const delegate_;
  // Owned by |AssistantManagerServiceImpl| which indirectly owns us.
  assistant_client::PlatformApi* const platform_api_;

  std::unique_ptr<assistant_client::AssistantManager> assistant_manager_;
  assistant_client::AssistantManagerInternal* assistant_manager_internal_ =
      nullptr;
  std::unique_ptr<assistant::LibassistantV1Api> libassistant_v1_api_;

  mojo::Receiver<mojom::ServiceController> receiver_;
  mojo::RemoteSet<mojom::StateObserver> state_observers_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_SERVICE_CONTROLLER_H_
