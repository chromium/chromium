// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/assistant_client_impl.h"

#include <memory>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/assistant/internal/internal_constants.h"
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
    : AssistantClientV1(std::move(assistant_manager),
                        assistant_manager_internal),
      grpc_services_(libassistant_service_address, assistant_service_address),
      client_(grpc_services_.GrpcLibassistantClient()),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

AssistantClientImpl::~AssistantClientImpl() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void AssistantClientImpl::StartServices() {
  StartGrpcServices();

  AssistantClientV1::StartServices();
}

bool AssistantClientImpl::StartGrpcServices() {
  return grpc_services_.Start();
}

void AssistantClientImpl::AddExperimentIds(
    const std::vector<std::string>& exp_ids) {}

void AssistantClientImpl::SendVoicelessInteraction(
    const ::assistant::api::Interaction& interaction,
    const std::string& description,
    const ::assistant::api::VoicelessOptions& options,
    base::OnceCallback<void(bool)> on_done) {
  NOTIMPLEMENTED();
}

// static
std::unique_ptr<AssistantClient> AssistantClient::Create(
    std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  if (chromeos::assistant::features::IsLibAssistantV2Enabled()) {
    // Note that we should *not* depend on |assistant_manager_internal| for V2,
    // so |assistant_manager_internal| will be nullptr after the migration has
    // done.
    return std::make_unique<AssistantClientImpl>(
        std::move(assistant_manager), assistant_manager_internal,
        assistant::kLibassistantServiceAddress,
        assistant::kAssistantServiceAddress);
  }

  return std::make_unique<AssistantClientV1>(std::move(assistant_manager),
                                             assistant_manager_internal);
}

}  // namespace libassistant
}  // namespace chromeos
