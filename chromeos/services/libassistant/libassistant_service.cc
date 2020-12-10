// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/libassistant_service.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "chromeos/services/libassistant/service_controller.h"

namespace chromeos {
namespace libassistant {

LibassistantService::LibassistantService(
    mojo::PendingReceiver<mojom::LibassistantService> receiver,
    assistant_client::PlatformApi* platform_api,
    assistant::AssistantManagerServiceDelegate* delegate)
    : platform_api_(platform_api),
      delegate_(delegate),
      receiver_(this, std::move(receiver)) {}

LibassistantService::~LibassistantService() = default;

assistant_client::AssistantManager* LibassistantService::assistant_manager() {
  DCHECK(service_controller_);
  return service_controller_->assistant_manager();
}

assistant_client::AssistantManagerInternal*
LibassistantService::assistant_manager_internal() {
  DCHECK(service_controller_);
  return service_controller_->assistant_manager_internal();
}

void LibassistantService::BindServiceController(
    mojo::PendingReceiver<mojom::ServiceController> receiver) {
  // Cannot bind the service controller twice.
  DCHECK(!service_controller_);
  service_controller_ = std::make_unique<ServiceController>(
      std::move(receiver), delegate_, platform_api_);
}

}  // namespace libassistant
}  // namespace chromeos
