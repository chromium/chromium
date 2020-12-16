// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_SERVICE_H_
#define CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_SERVICE_H_

#include "chromeos/services/assistant/test_support/fake_service_controller.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace assistant {

// Fake implementation of the Libassistant Mojom service.
// It allows hooks to read and control the state of the service.
class FakeLibassistantService
    : public libassistant::mojom::LibassistantService {
 public:
  FakeLibassistantService();
  FakeLibassistantService(FakeLibassistantService&) = delete;
  FakeLibassistantService& operator=(FakeLibassistantService&) = delete;
  ~FakeLibassistantService() override;

  void Bind(mojo::PendingReceiver<libassistant::mojom::LibassistantService>
                pending_receiver);
  void Unbind();

  FakeServiceController& service_controller() { return service_controller_; }

  // mojom::LibassistantService implementation:
  void BindServiceController(
      mojo::PendingReceiver<libassistant::mojom::ServiceController> receiver)
      override;
  void BindAudioInputController() override {}
  void BindAudioOutputController() override {}
  void BindInteractionController() override {}

 private:
  mojo::Receiver<libassistant::mojom::LibassistantService> receiver_;

  FakeServiceController service_controller_;
};

}  // namespace assistant
}  // namespace chromeos
#endif  // CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_SERVICE_H_
