// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/assistant_client_impl.h"

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/services/libassistant/grpc/grpc_libassistant_client.h"

namespace chromeos {
namespace libassistant {

AssistantClientImpl::AssistantClientImpl(
    const std::string& libassistant_service_address)
    : grpc_services_(libassistant_service_address),
      client_(grpc_services_.GrpcLibassistantClient()),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

AssistantClientImpl::~AssistantClientImpl() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

bool AssistantClientImpl::StartGrpcServices() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace libassistant
}  // namespace chromeos
