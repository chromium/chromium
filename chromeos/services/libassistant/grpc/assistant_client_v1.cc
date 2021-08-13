// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/assistant_client_v1.h"

#include "base/callback.h"
#include "chromeos/assistant/internal/grpc_transport/request_utils.h"
#include "chromeos/assistant/internal/proto/shared/proto/conversation.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/speaker_id_enrollment_event.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/speaker_id_enrollment_interface.pb.h"
#include "chromeos/services/libassistant/callback_utils.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/internal_api/speaker_id_enrollment.h"

namespace chromeos {
namespace libassistant {

using SpeakerIdEnrollmentEvent =
    ::assistant::api::events::SpeakerIdEnrollmentEvent;
using SpeakerIdEnrollmentUpdate = assistant_client::SpeakerIdEnrollmentUpdate;

namespace {

SpeakerIdEnrollmentEvent ConvertToGrpcEvent(
    const ::assistant_client::SpeakerIdEnrollmentUpdate::State& state) {
  SpeakerIdEnrollmentEvent event;
  switch (state) {
    case SpeakerIdEnrollmentUpdate::State::INIT: {
      event.mutable_init_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::CHECK: {
      event.mutable_check_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::LISTEN: {
      event.mutable_listen_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::PROCESS: {
      event.mutable_process_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::UPLOAD: {
      event.mutable_upload_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::FETCH: {
      event.mutable_fetch_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::DONE: {
      event.mutable_done_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::FAILURE: {
      event.mutable_failure_state();
      break;
    }
  }
  return event;
}

}  // namespace

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

void AssistantClientV1::StartSpeakerIdEnrollment(
    const StartSpeakerIdEnrollmentRequest& request,
    base::RepeatingCallback<void(const SpeakerIdEnrollmentEvent&)> on_done) {
  assistant_client::SpeakerIdEnrollmentConfig client_config;
  client_config.user_id = request.user_id();
  client_config.skip_cloud_enrollment = request.skip_cloud_enrollment();

  auto callback = base::BindRepeating(
      [](base::RepeatingCallback<void(const SpeakerIdEnrollmentEvent&)>
             callback,
         const SpeakerIdEnrollmentUpdate& update) {
        callback.Run(ConvertToGrpcEvent(update.state));
      },
      std::move(on_done));

  assistant_manager_internal()->StartSpeakerIdEnrollment(
      client_config,
      ToStdFunctionRepeating(BindToCurrentSequenceRepeating(callback)));
}

void AssistantClientV1::CancelSpeakerIdEnrollment(
    const CancelSpeakerIdEnrollmentRequest& request) {
  assistant_manager_internal()->StopSpeakerIdEnrollment([]() {});
}

void AssistantClientV1::GetSpeakerIdEnrollmentInfo(
    const ::assistant::api::GetSpeakerIdEnrollmentInfoRequest& request,
    base::OnceCallback<void(bool user_model_exists)> on_done) {
  auto callback = base::BindOnce(
      [](base::OnceCallback<void(bool user_model_exists)> cb,
         const assistant_client::SpeakerIdEnrollmentStatus& status) {
        std::move(cb).Run(status.user_model_exists);
      },
      std::move(on_done));

  assistant_manager_internal()->GetSpeakerIdEnrollmentStatus(
      request.cloud_enrollment_status_request().user_id(),
      ToStdFunction(BindToCurrentSequence(std::move(callback))));
}

}  // namespace libassistant
}  // namespace chromeos
