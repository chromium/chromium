// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_delegate_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "chromeos/services/assistant/platform/audio_input_host_impl.h"
#include "chromeos/services/assistant/proxy/assistant_proxy.h"
#include "chromeos/services/assistant/service_context.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/assistant_manager.h"

namespace chromeos {
namespace assistant {

AssistantManagerServiceDelegateImpl::AssistantManagerServiceDelegateImpl(
    ServiceContext* context)
    : context_(context) {}

AssistantManagerServiceDelegateImpl::~AssistantManagerServiceDelegateImpl() =
    default;

std::unique_ptr<AudioInputHost>
AssistantManagerServiceDelegateImpl::CreateAudioInputHost(
    mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
        pending_remote) {
  return std::make_unique<AudioInputHostImpl>(
      std::move(pending_remote), context_->cras_audio_handler(),
      context_->power_manager_client(),
      context_->assistant_state()->locale().value());
}

std::unique_ptr<assistant_client::AssistantManager>
AssistantManagerServiceDelegateImpl::CreateAssistantManager(
    assistant_client::PlatformApi* platform_api,
    const std::string& lib_assistant_config) {
  // This circumnvent way of creating the unique_ptr is required because
  // |AssistantManager::Create| returns a raw pointer, and passing that in the
  // constructor of unique_ptr is blocked by our presubmit checks that try to
  // force us to use make_unique, which we can't use here.
  std::unique_ptr<assistant_client::AssistantManager> result;
  result.reset(assistant_client::AssistantManager::Create(
      platform_api, lib_assistant_config));
  return result;
}

assistant_client::AssistantManagerInternal*
AssistantManagerServiceDelegateImpl::UnwrapAssistantManagerInternal(
    assistant_client::AssistantManager* assistant_manager) {
  return assistant_client::UnwrapAssistantManagerInternal(assistant_manager);
}

}  // namespace assistant
}  // namespace chromeos
