// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
class PlatformApi;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {
class AssistantManagerServiceDelegate;
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

class ServiceController;

class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) LibassistantService
    : public mojom::LibassistantService {
 public:
  using InitializeCallback =
      base::OnceCallback<void(assistant_client::AssistantManager*,
                              assistant_client::AssistantManagerInternal*)>;

  explicit LibassistantService(
      mojo::PendingReceiver<mojom::LibassistantService> receiver,
      assistant_client::PlatformApi* platform_api,
      assistant::AssistantManagerServiceDelegate* delegate);
  LibassistantService(LibassistantService&) = delete;
  LibassistantService& operator=(LibassistantService&) = delete;
  ~LibassistantService() override;

  void SetInitializeCallback(InitializeCallback callback);

 private:
  ServiceController& service_controller() { return *service_controller_; }

  // mojom::LibassistantService implementation:
  void BindServiceController(
      mojo::PendingReceiver<mojom::ServiceController> receiver) override;
  void BindAudioInputController() override {}
  void BindAudioOutputController() override {}
  void BindInteractionController() override {}

  mojo::Receiver<mojom::LibassistantService> receiver_;
  std::unique_ptr<ServiceController> service_controller_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_
