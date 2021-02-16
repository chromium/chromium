// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_

#include <memory>

#include "base/single_thread_task_runner.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom-forward.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
class PlatformApi;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

class AudioInputHost;

// Interface class that provides factory methods for assistant internal
// functionality.
class AssistantManagerServiceDelegate {
 public:
  AssistantManagerServiceDelegate() = default;
  AssistantManagerServiceDelegate(const AssistantManagerServiceDelegate&) =
      delete;
  AssistantManagerServiceDelegate& operator=(
      const AssistantManagerServiceDelegate&) = delete;
  virtual ~AssistantManagerServiceDelegate() = default;

  virtual std::unique_ptr<AudioInputHost> CreateAudioInputHost(
      mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
          pending_remote) = 0;

  virtual std::unique_ptr<assistant_client::AssistantManager>
  CreateAssistantManager(assistant_client::PlatformApi* platform_api,
                         const std::string& lib_assistant_config) = 0;

  virtual assistant_client::AssistantManagerInternal*
  UnwrapAssistantManagerInternal(
      assistant_client::AssistantManager* assistant_manager) = 0;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_
