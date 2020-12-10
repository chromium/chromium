// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_FAKE_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_FAKE_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_

#include "base/component_export.h"

#include "chromeos/services/assistant/public/cpp/migration/assistant_manager_service_delegate.h"

namespace chromeos {
namespace assistant {

class FakeAssistantManager;

// Implementation of |AssistantManagerServiceDelegate| that returns fake
// instances for all of the member methods. Used during unittests.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_MIGRATION_TEST_SUPPORT)
    FakeAssistantManagerServiceDelegate
    : public AssistantManagerServiceDelegate {
 public:
  FakeAssistantManagerServiceDelegate();
  ~FakeAssistantManagerServiceDelegate() override;

  FakeAssistantManager* assistant_manager();

  // AssistantManagerServiceDelegate:
  std::unique_ptr<CrosPlatformApi> CreatePlatformApi(
      AssistantMediaSession* media_session,
      scoped_refptr<base::SingleThreadTaskRunner> background_thread_task_runner)
      override;
  std::unique_ptr<assistant_client::AssistantManager> CreateAssistantManager(
      assistant_client::PlatformApi* platform_api,
      const std::string& libassistant_config) override;
  assistant_client::AssistantManagerInternal* UnwrapAssistantManagerInternal(
      assistant_client::AssistantManager* assistant_manager) override;

  std::string libassistant_config() const { return libassistant_config_; }

 private:
  std::unique_ptr<FakeAssistantManager> pending_assistant_manager_;
  FakeAssistantManager* current_assistant_manager_ = nullptr;

  // Config passed to LibAssistant when it was started.
  std::string libassistant_config_;

  DISALLOW_COPY_AND_ASSIGN(FakeAssistantManagerServiceDelegate);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_FAKE_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_
