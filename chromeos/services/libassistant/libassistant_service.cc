// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/libassistant_service.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "chromeos/services/assistant/public/cpp/migration/cros_platform_api.h"
#include "chromeos/services/libassistant/platform_api.h"
#include "chromeos/services/libassistant/service_controller.h"

namespace chromeos {
namespace libassistant {

LibassistantService::LibassistantService(
    mojo::PendingReceiver<mojom::LibassistantService> receiver,
    chromeos::assistant::CrosPlatformApi* platform_api,
    assistant::AssistantManagerServiceDelegate* delegate)
    : receiver_(this, std::move(receiver)),
      platform_api_(std::make_unique<PlatformApi>()),
      service_controller_(
          std::make_unique<ServiceController>(delegate, platform_api_.get())) {
  platform_api_->SetAudioInputProvider(&platform_api->GetAudioInputProvider())
      .SetAudioOutputProvider(&platform_api->GetAudioOutputProvider())
      .SetAuthProvider(&platform_api->GetAuthProvider())
      .SetFileProvider(&platform_api->GetFileProvider())
      .SetNetworkProvider(&platform_api->GetNetworkProvider())
      .SetSystemProvider(&platform_api->GetSystemProvider());
}

LibassistantService::~LibassistantService() = default;

void LibassistantService::BindServiceController(
    mojo::PendingReceiver<mojom::ServiceController> receiver) {
  service_controller_->Bind(std::move(receiver));
}

void LibassistantService::SetInitializeCallback(InitializeCallback callback) {
  service_controller().SetInitializeCallback(std::move(callback));
}

}  // namespace libassistant
}  // namespace chromeos
