// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
class PlatformApi;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

class AssistantMediaSession;
class CrosPlatformApi;

// Interface class that provides factory methods for assistant internal
// functionality.
class AssistantManagerServiceDelegate {
 public:
  AssistantManagerServiceDelegate() = default;
  virtual ~AssistantManagerServiceDelegate() = default;

  virtual std::unique_ptr<CrosPlatformApi> CreatePlatformApi(
      AssistantMediaSession* media_session,
      scoped_refptr<base::SingleThreadTaskRunner>
          background_thread_task_runner) = 0;

  virtual std::unique_ptr<assistant_client::AssistantManager>
  CreateAssistantManager(assistant_client::PlatformApi* platform_api,
                         const std::string& lib_assistant_config) = 0;

  virtual assistant_client::AssistantManagerInternal*
  UnwrapAssistantManagerInternal(
      assistant_client::AssistantManager* assistant_manager) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantManagerServiceDelegate);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_DELEGATE_H_
