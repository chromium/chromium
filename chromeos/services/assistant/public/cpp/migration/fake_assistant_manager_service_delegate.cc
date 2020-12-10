// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chromeos/services/assistant//public/cpp/migration/fake_assistant_manager_service_delegate.h"

#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/services/assistant//public/cpp/migration/fake_platform_api.h"

namespace chromeos {
namespace assistant {

FakeAssistantManagerServiceDelegate::FakeAssistantManagerServiceDelegate() {
  // We start by creating a pending assistant manager, as our unittests
  // might need to access the assistant manager before it is created through
  // CreateAssistantManager() (for example to set expectations).
  pending_assistant_manager_ = std::make_unique<FakeAssistantManager>();
}

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
    const std::string& libassistant_config) {
  auto result = std::move(pending_assistant_manager_);
  if (!result) {
    // We come here if this is not the first call to CreateAssistantManager().
    result = std::make_unique<FakeAssistantManager>();
  }

  // Keep a pointer around so our unittests can still retrieve it.
  current_assistant_manager_ = result.get();
  libassistant_config_ = libassistant_config;

  return result;
}

assistant_client::AssistantManagerInternal*
FakeAssistantManagerServiceDelegate::UnwrapAssistantManagerInternal(
    assistant_client::AssistantManager* assistant_manager) {
  DCHECK(current_assistant_manager_);
  return &current_assistant_manager_->assistant_manager_internal();
}

FakeAssistantManager* FakeAssistantManagerServiceDelegate::assistant_manager() {
  if (current_assistant_manager_)
    return current_assistant_manager_;

  // We should only come here if CreateAssistantManager() has not been called
  // yet. In that case we return a pointer to the pending assistant manager
  // instead.
  DCHECK(pending_assistant_manager_);
  return pending_assistant_manager_.get();
}

}  // namespace assistant
}  // namespace chromeos
