// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/assistant_client_impl.h"

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/libassistant/grpc/assistant_client_v1.h"
#include "chromeos/services/libassistant/grpc/grpc_libassistant_client.h"
#include "libassistant/shared/public/assistant_manager.h"

namespace chromeos {
namespace libassistant {

AssistantClientImpl::AssistantClientImpl(
    std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal,
    const std::string& libassistant_service_address,
    const std::string& assistant_service_address)
    : AssistantClient(std::move(assistant_manager), assistant_manager_internal),
      grpc_services_(libassistant_service_address, assistant_service_address),
      client_(grpc_services_.GrpcLibassistantClient()),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

AssistantClientImpl::~AssistantClientImpl() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

bool AssistantClientImpl::StartGrpcServices() {
  return grpc_services_.Start();
}

void AssistantClientImpl::AddExperimentIds(
    const std::vector<std::string>& exp_ids) {}

// static
std::unique_ptr<AssistantClient> AssistantClient::Create(
    std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  if (chromeos::assistant::features::IsLibAssistantV2Enabled()) {
    // not supported yet
    return nullptr;
  }

  return std::make_unique<AssistantClientV1>(std::move(assistant_manager),
                                             assistant_manager_internal);
}

}  // namespace libassistant
}  // namespace chromeos
