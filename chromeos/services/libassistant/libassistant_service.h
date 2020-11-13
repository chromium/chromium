// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace libassistant {

class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) LibassistantService
    : public mojom::LibassistantService {
 public:
  explicit LibassistantService(
      mojo::PendingReceiver<mojom::LibassistantService> receiver);
  LibassistantService(LibassistantService&) = delete;
  LibassistantService& operator=(LibassistantService&) = delete;
  ~LibassistantService() override;

 private:
  // mojom::LibassistantService implementation:
  void BindServiceController(
      mojo::PendingReceiver<mojom::ServiceController> receiver) override;
  void BindAudioInputController() override {}
  void BindAudioOutputController() override {}
  void BindInteractionController() override {}

  mojo::Receiver<mojom::LibassistantService> receiver_;
  std::unique_ptr<mojom::ServiceController> service_controller_;
};

class ServiceController : public mojom::ServiceController {
 public:
  explicit ServiceController(
      mojo::PendingReceiver<mojom::ServiceController> receiver);
  ServiceController(ServiceController&) = delete;
  ServiceController& operator=(ServiceController&) = delete;
  ~ServiceController() override;

 private:
  mojo::Receiver<mojom::ServiceController> receiver_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_
