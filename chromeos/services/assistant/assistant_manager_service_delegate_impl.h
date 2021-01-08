// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_DELEGATE_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_DELEGATE_IMPL_H_

#include <memory>
#include <string>

#include "chromeos/services/assistant/public/cpp/migration/assistant_manager_service_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"

namespace chromeos {
namespace assistant {

class ServiceContext;

class AssistantManagerServiceDelegateImpl
    : public AssistantManagerServiceDelegate {
 public:
  AssistantManagerServiceDelegateImpl(
      mojo::PendingRemote<device::mojom::BatteryMonitor> battery_monitor,
      ServiceContext* context);
  ~AssistantManagerServiceDelegateImpl() override;

  std::unique_ptr<CrosPlatformApi> CreatePlatformApi(
      AssistantMediaSession* media_session,
      scoped_refptr<base::SingleThreadTaskRunner> background_thread_task_runner)
      override;

  std::unique_ptr<assistant_client::AssistantManager> CreateAssistantManager(
      assistant_client::PlatformApi* platform_api,
      const std::string& lib_assistant_config) override;

  assistant_client::AssistantManagerInternal* UnwrapAssistantManagerInternal(
      assistant_client::AssistantManager* assistant_manager) override;

 private:
  mojo::PendingRemote<device::mojom::BatteryMonitor> battery_monitor_;
  // Owned by the parent |Service| which will destroy |this| before |context_|.
  ServiceContext* context_;

  DISALLOW_COPY_AND_ASSIGN(AssistantManagerServiceDelegateImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_DELEGATE_IMPL_H_
