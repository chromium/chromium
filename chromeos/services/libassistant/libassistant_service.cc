// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/libassistant_service.h"

#include <memory>
#include <utility>

#include "base/logging.h"

namespace chromeos {
namespace libassistant {

LibassistantService::LibassistantService(
    mojo::PendingReceiver<mojom::LibassistantService> receiver)
    : receiver_(this, std::move(receiver)) {}

LibassistantService::~LibassistantService() = default;

void LibassistantService::BindServiceController(
    mojo::PendingReceiver<mojom::ServiceController> receiver) {
  DCHECK(!service_controller_);
  service_controller_ =
      std::make_unique<ServiceController>(std::move(receiver));
}

ServiceController::ServiceController(
    mojo::PendingReceiver<mojom::ServiceController> receiver)
    : receiver_(this, std::move(receiver)) {}

ServiceController::~ServiceController() = default;

}  // namespace libassistant
}  // namespace chromeos
