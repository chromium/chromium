// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_FAKE_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_FAKE_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_

#include "base/component_export.h"

#include <memory>
#include <string>

#include "chromeos/services/assistant/public/cpp/migration/assistant_manager_service_delegate.h"

namespace chromeos {
namespace assistant {

class FakeAssistantManager;
class FakeAssistantManagerInternal;

// Implementation of |AssistantManagerServiceDelegate| that returns fake
// instances for all of the member methods. Used during unittests.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_MIGRATION_TEST_SUPPORT)
    FakeAssistantManagerServiceDelegate
    : public AssistantManagerServiceDelegate {
 public:
  FakeAssistantManagerServiceDelegate();
  ~FakeAssistantManagerServiceDelegate() override;

  FakeAssistantManager* assistant_manager() { return assistant_manager_ptr_; }

  FakeAssistantManagerInternal* assistant_manager_internal() {
    return assistant_manager_internal_.get();
  }

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
  // Will be initialized in the constructor and moved out when
  // |CreateAssistantManager| is called.
  std::unique_ptr<FakeAssistantManager> assistant_manager_;
  std::unique_ptr<FakeAssistantManagerInternal> assistant_manager_internal_;
  FakeAssistantManager* assistant_manager_ptr_;

  // Config passed to LibAssistant when it was started.
  std::string libassistant_config_;

  DISALLOW_COPY_AND_ASSIGN(FakeAssistantManagerServiceDelegate);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_FAKE_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_
