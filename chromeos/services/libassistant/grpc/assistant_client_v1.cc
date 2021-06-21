// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/assistant_client_v1.h"

#include "libassistant/shared/internal_api/assistant_manager_internal.h"

namespace chromeos {
namespace libassistant {

AssistantClientV1::AssistantClientV1(
    std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal)
    : AssistantClient(std::move(assistant_manager),
                      assistant_manager_internal) {}

AssistantClientV1::~AssistantClientV1() = default;

bool AssistantClientV1::StartGrpcServices() {
  return true;
}

void AssistantClientV1::AddExperimentIds(
    const std::vector<std::string>& exp_ids) {
  assistant_manager_internal()->AddExtraExperimentIds(exp_ids);
}

}  // namespace libassistant
}  // namespace chromeos
