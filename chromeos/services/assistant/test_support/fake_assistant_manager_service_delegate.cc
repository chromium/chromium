// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/test_support/fake_assistant_manager_service_delegate.h"

#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/services/assistant/test_support/fake_platform_api.h"

namespace chromeos {
namespace assistant {

FakeAssistantManagerServiceDelegate::FakeAssistantManagerServiceDelegate()
    : assistant_manager_(std::make_unique<FakeAssistantManager>()),
      assistant_manager_internal_(
          std::make_unique<FakeAssistantManagerInternal>()),
      assistant_manager_ptr_(assistant_manager_.get()) {}

FakeAssistantManagerServiceDelegate::~FakeAssistantManagerServiceDelegate() =
    default;

std::unique_ptr<CrosPlatformApi>
FakeAssistantManagerServiceDelegate::CreatePlatformApi(
    AssistantMediaSession* media_session,
    scoped_refptr<base::SingleThreadTaskRunner> background_thread_task_runner) {
  return std::make_unique<FakePlatformApi>();
}

std::unique_ptr<assistant_client::AssistantManager>
FakeAssistantManagerServiceDelegate::CreateAssistantManager(
    assistant_client::PlatformApi* platform_api,
    const std::string& lib_assistant_config) {
  DCHECK(assistant_manager_);
  return std::move(assistant_manager_);
}

assistant_client::AssistantManagerInternal*
FakeAssistantManagerServiceDelegate::UnwrapAssistantManagerInternal(
    assistant_client::AssistantManager* assistant_manager) {
  return assistant_manager_internal_.get();
}

}  // namespace assistant
}  // namespace chromeos
