// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/assistant_client_v1.h"

#include "base/callback.h"
#include "chromeos/assistant/internal/grpc_transport/request_utils.h"
#include "chromeos/assistant/internal/proto/shared/proto/conversation.pb.h"
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

void AssistantClientV1::SendVoicelessInteraction(
    const ::assistant::api::Interaction& interaction,
    const std::string& description,
    const ::assistant::api::VoicelessOptions& options,
    base::OnceCallback<void(bool)> on_done) {
  assistant_client::VoicelessOptions voiceless_options;
  PopulateVoicelessOptionsFromProto(options, &voiceless_options);
  assistant_manager_internal()->SendVoicelessInteraction(
      interaction.SerializeAsString(), description, voiceless_options,
      [callback = std::move(on_done)](bool result) mutable {
        std::move(callback).Run(result);
      });
}

}  // namespace libassistant
}  // namespace chromeos
