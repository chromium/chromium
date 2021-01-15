// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/libassistant_service.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "chromeos/services/assistant/public/cpp/migration/cros_platform_api.h"
#include "chromeos/services/libassistant/audio_input_controller.h"
#include "chromeos/services/libassistant/conversation_controller.h"
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
          std::make_unique<ServiceController>(delegate, platform_api_.get())),
      conversation_controller_(
          std::make_unique<ConversationController>(service_controller_.get())),
      audio_input_controller_(std::make_unique<AudioInputController>()) {
  platform_api_->SetAudioInputProvider(&platform_api->GetAudioInputProvider())
      .SetAudioOutputProvider(&platform_api->GetAudioOutputProvider())
      .SetAuthProvider(&platform_api->GetAuthProvider())
      .SetFileProvider(&platform_api->GetFileProvider())
      .SetNetworkProvider(&platform_api->GetNetworkProvider())
      .SetSystemProvider(&platform_api->GetSystemProvider());
}

LibassistantService::~LibassistantService() = default;

void LibassistantService::Bind(
    mojo::PendingReceiver<mojom::AudioInputController> audio_input_controller,
    mojo::PendingRemote<mojom::AudioStreamFactoryDelegate>
        audio_stream_factory_delegate,
    mojo::PendingReceiver<mojom::ConversationController>
        conversation_controller,
    mojo::PendingReceiver<mojom::ServiceController> service_controller) {
  audio_input_controller_->Bind(std::move(audio_input_controller),
                                std::move(audio_stream_factory_delegate));
  service_controller_->Bind(std::move(service_controller));
  conversation_controller_->Bind(std::move(conversation_controller));
}

void LibassistantService::SetInitializeCallback(InitializeCallback callback) {
  service_controller().SetInitializeCallback(std::move(callback));
}

}  // namespace libassistant
}  // namespace chromeos
