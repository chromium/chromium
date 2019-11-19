// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_delegate_impl.h"

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "chromeos/services/assistant/platform_api_impl.h"
#include "chromeos/services/assistant/service_context.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/assistant_manager.h"

namespace chromeos {
namespace assistant {

AssistantManagerServiceDelegateImpl::AssistantManagerServiceDelegateImpl(
    mojo::PendingRemote<device::mojom::BatteryMonitor> battery_monitor,
    mojom::Client* client,
    ServiceContext* context)
    : battery_monitor_(std::move(battery_monitor)),
      client_(client),
      context_(context) {}

AssistantManagerServiceDelegateImpl::~AssistantManagerServiceDelegateImpl() =
    default;

std::unique_ptr<CrosPlatformApi>
AssistantManagerServiceDelegateImpl::CreatePlatformApi(
    AssistantMediaSession* media_session,
    scoped_refptr<base::SingleThreadTaskRunner> background_thread_task_runner) {
  return std::make_unique<PlatformApiImpl>(
      client_, media_session, context_->power_manager_client(),
      context_->cras_audio_handler(), std::move(battery_monitor_),
      context_->main_task_runner(), background_thread_task_runner,
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
