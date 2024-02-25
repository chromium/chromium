// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/assistant_client_impl.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/action_service.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_libassistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_util.h"
#include "chromeos/ash/services/libassistant/grpc/services_status_observer.h"
#include "chromeos/ash/services/libassistant/grpc/utils/media_status_utils.h"
#include "chromeos/ash/services/libassistant/grpc/utils/timer_utils.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_timer.h"
#include "chromeos/assistant/internal/grpc_transport/request_utils.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/settings_ui.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/alarm_timer_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/audio_utils_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/bootup_settings_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/config_settings_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/device_state_event.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/display_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/experiment_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/query_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/speaker_id_enrollment_interface.pb.h"

namespace ash::libassistant {

namespace {

using ::assistant::api::EnableListeningRequest;
using ::assistant::api::EnableListeningResponse;
using ::assistant::api::GetAssistantSettingsResponse;
using ::assistant::api::OnAlarmTimerEventRequest;
using ::assistant::api::OnDeviceStateEventRequest;
using ::assistant::api::OnSpeakerIdEnrollmentEventRequest;
using ::assistant::api::SetLocaleOverrideRequest;
using ::assistant::api::SetLocaleOverrideResponse;
using ::assistant::api::UpdateAssistantSettingsResponse;
using ::assistant::ui::SettingsUiUpdate;

// Rpc call config constants.
constexpr int kMaxRpcRetries = 5;
constexpr int kDefaultTimeoutMs = 5000;

// Interaction related calls has longer timeout.
constexpr int kAssistantInteractionDefaultTimeoutMs = 20000;

const StateConfig kDefaultStateConfig =
    StateConfig{kMaxRpcRetries, kDefaultTimeoutMs};

const StateConfig kInteractionDefaultStateConfig =
    StateConfig{kMaxRpcRetries, kAssistantInteractionDefaultTimeoutMs};

#define LOG_GRPC_STATUS(level, status, func_name)                \
  if (status.ok()) {                                             \
    DVLOG(level) << func_name << " succeed with ok status.";     \
  } else {                                                       \
    LOG(ERROR) << func_name << " failed with a non-ok status.";  \
    LOG(ERROR) << "Error code: " << status.error_code()          \
               << ", error message: " << status.error_message(); \
  }

// Creates a callback for logging the request status. The callback will
// ignore the returned response as it either doesn't contain any information
// we need or is empty.
template <typename Response>
base::OnceCallback<void(const grpc::Status& status, const Response&)>
GetLoggingCallback(const std::string& request_name) {
  return base::BindOnce(
      [](const std::string& request_name, const grpc::Status& status,
         const Response& ignored) { LOG_GRPC_STATUS(2, status, request_name); },
      request_name);
}

}  // namespace

AssistantClientImpl::AssistantClientImpl(
    std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
    const std::string& libassistant_service_address,
    const std::string& assistant_service_address)
    : AssistantClient(std::move(assistant_manager)),
      grpc_services_(libassistant_service_address, assistant_service_address),
      libassistant_client_(grpc_services_.GrpcLibassistantClient()) {}

AssistantClientImpl::~AssistantClientImpl() {
  // The following sequence is used to prevent unnecessary heart beats from
  // being sent during shutdown:
  // 1. Stop other LibAssistant gRPC services by destroying the
  // `assistant_manager_`.
  // 2. Stop assistant_grpc service by destroying `grpc_services_`.
  ResetAssistantManager();
}

void AssistantClientImpl::StartServices(
    ServicesStatusObserver* services_status_observer) {
  grpc_services_.GetServicesStatusProvider().AddObserver(
      services_status_observer);

  StartGrpcServices();
}

bool AssistantClientImpl::StartGrpcServices() {
  return grpc_services_.Start();
}

void AssistantClientImpl::StartGrpcHttpConnectionClient(
    assistant_client::HttpConnectionFactory* factory) {
  grpc_services_.StartGrpcHttpConnectionClient(factory);
}

void AssistantClientImpl::AddExperimentIds(
    const std::vector<std::string>& exp_ids) {
  ::assistant::api::UpdateExperimentIdsRequest request;
  request.set_operation(
      ::assistant::api::UpdateExperimentIdsRequest_Operation_MERGE);
  *request.mutable_experiment_ids() = {exp_ids.begin(), exp_ids.end()};

  libassistant_client_->CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::UpdateExperimentIdsResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::AddSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {
  grpc_services_.AddSpeakerIdEnrollmentEventObserver(observer);
}

void AssistantClientImpl::RemoveSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {
  grpc_services_.RemoveSpeakerIdEnrollmentEventObserver(observer);
}

void AssistantClientImpl::StartSpeakerIdEnrollment(
    const StartSpeakerIdEnrollmentRequest& request) {
  libassistant_client_->CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::StartSpeakerIdEnrollmentResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::CancelSpeakerIdEnrollment(
    const CancelSpeakerIdEnrollmentRequest& request) {
  libassistant_client_->CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::CancelSpeakerIdEnrollmentResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::GetSpeakerIdEnrollmentInfo(
    const GetSpeakerIdEnrollmentInfoRequest& request,
    base::OnceCallback<void(bool user_model_exists)> on_done) {
  libassistant_client_->CallServiceMethod(
      request,
      base::BindOnce(
          [](base::OnceCallback<void(bool user_model_exists)> on_done,
             const grpc::Status& status,
             const ::assistant::api::GetSpeakerIdEnrollmentInfoResponse&
                 response) {
            bool has_model = false;
            //  `response` could have an error field.
            // Treat any error as no existing model.
            if (response.has_user_model_status_response()) {
              has_model =
                  response.user_model_status_response().user_model_exists();
            }
            std::move(on_done).Run(has_model);
          },
          std::move(on_done)),
      kDefaultStateConfig);
}

void AssistantClientImpl::ResetAllDataAndShutdown() {
  assistant_manager()->ResetAllDataAndShutdown();
}

void AssistantClientImpl::SendDisplayRequest(
    const OnDisplayRequestRequest& request) {
  libassistant_client_->CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::OnDisplayRequestResponse>(
          /*request_name=*/__func__),
      kInteractionDefaultStateConfig);
}

void AssistantClientImpl::AddDisplayEventObserver(
    GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) {
  grpc_services_.AddAssistantDisplayEventObserver(observer);
}

void AssistantClientImpl::ResumeCurrentStream() {
  assistant_manager()->GetMediaManager()->Resume();
}

void AssistantClientImpl::PauseCurrentStream() {
  assistant_manager()->GetMediaManager()->Pause();
}

void AssistantClientImpl::SetExternalPlaybackState(
    const MediaStatus& status_proto) {
  assistant_client::MediaStatus media_status;
  ConvertMediaStatusToV1FromV2(status_proto, &media_status);
  assistant_manager()->GetMediaManager()->SetExternalPlaybackState(
      media_status);
}

void AssistantClientImpl::AddDeviceStateEventObserver(
    GrpcServicesObserver<OnDeviceStateEventRequest>* observer) {
  grpc_services_.AddDeviceStateEventObserver(observer);
}

void AssistantClientImpl::AddMediaActionFallbackEventObserver(
    GrpcServicesObserver<OnMediaActionFallbackEventRequest>* observer) {
  grpc_services_.AddMediaActionFallbackEventObserver(observer);
}

void AssistantClientImpl::SendVoicelessInteraction(
    const ::assistant::api::Interaction& interaction,
    const std::string& description,
    const ::assistant::api::VoicelessOptions& options,
    base::OnceCallback<void(bool)> on_done) {
  ::assistant::api::SendQueryRequest request;
  chromeos::libassistant::PopulateSendQueryRequest(interaction, description,
                                                   options, &request);

  libassistant_client_->CallServiceMethod(
      request,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> on_done, const grpc::Status& status,
             const ::assistant::api::SendQueryResponse& response) {
            std::move(on_done).Run(response.success());
          },
          std::move(on_done)),
      kInteractionDefaultStateConfig);
}

void AssistantClientImpl::RegisterActionModule(
    assistant_client::ActionModule* action_module) {
  grpc_services_.GetActionService()->RegisterActionModule(action_module);
}

void AssistantClientImpl::StartVoiceInteraction() {
  libassistant_client_->CallServiceMethod(
      ::assistant::api::StartVoiceQueryRequest(),
      GetLoggingCallback<::assistant::api::StartVoiceQueryResponse>(
          /*request_name=*/__func__),
      kInteractionDefaultStateConfig);
}

void AssistantClientImpl::StopAssistantInteraction(bool cancel_conversation) {
  ::assistant::api::StopQueryRequest request;
  request.set_type(::assistant::api::StopQueryRequest::ACTIVE_INTERNAL);
  request.set_cancel_conversation(cancel_conversation);

  libassistant_client_->CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::StopQueryResponse>(
          /*request_name=*/__func__),
      kInteractionDefaultStateConfig);
}

void AssistantClientImpl::AddConversationStateEventObserver(
    GrpcServicesObserver<OnConversationStateEventRequest>* observer) {
  grpc_services_.AddConversationStateEventObserver(observer);
}

void AssistantClientImpl::SetAuthenticationInfo(const AuthTokens& tokens) {
  ::assistant::api::SetAuthInfoRequest request;
  // Each token exists of a [gaia_id, auth_token] tuple.
  for (const auto& token : tokens) {
    auto* proto = request.add_tokens();
    proto->set_user_id(token.first);
    proto->set_auth_token(token.second);
  }

  libassistant_client_->CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::SetAuthInfoResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::SetInternalOptions(const std::string& locale,
                                             bool spoken_feedback_enabled) {
  auto internal_options = chromeos::assistant::CreateInternalOptionsProto(
      locale, spoken_feedback_enabled);

  ::assistant::api::SetInternalOptionsRequest request;
  *request.mutable_internal_options() = std::move(internal_options);

  // SetInternalOptions request causes AssistantManager reconfiguration.
  constexpr int kAssistantReconfigureInternalDefaultTimeoutMs = 20000;
  StateConfig custom_config(kMaxRpcRetries,
                            kAssistantReconfigureInternalDefaultTimeoutMs);
  libassistant_client_->CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::SetInternalOptionsResponse>(
          /*request_name=*/__func__),
      custom_config);
}

void AssistantClientImpl::UpdateAssistantSettings(
    const ::assistant::ui::SettingsUiUpdate& update,
    const std::string& user_id,
    base::OnceCallback<void(
        const ::assistant::api::UpdateAssistantSettingsResponse&)> on_done) {
  using ::assistant::api::UpdateAssistantSettingsRequest;
  using ::assistant::api::UpdateAssistantSettingsResponse;

  UpdateAssistantSettingsRequest request;
  // Sets obfuscated gaia id.
  request.set_user_id(user_id);
  // Sets the update to be applied to the settings.
  *request.mutable_update_settings_ui_request()->mutable_settings_update() =
      update;

  auto cb = base::BindOnce(
      [](base::OnceCallback<void(const UpdateAssistantSettingsResponse&)>
             on_done,
         const grpc::Status& status,
         const UpdateAssistantSettingsResponse& response) {
        LOG_GRPC_STATUS(/*level=*/2, status, "UpdateAssistantSettings")
        std::move(on_done).Run(response);
      },
      std::move(on_done));
  libassistant_client_->CallServiceMethod(request, std::move(cb),
                                          kDefaultStateConfig);
}

void AssistantClientImpl::GetAssistantSettings(
    const ::assistant::ui::SettingsUiSelector& selector,
    const std::string& user_id,
    base::OnceCallback<
        void(const ::assistant::api::GetAssistantSettingsResponse&)> on_done) {
  using ::assistant::api::GetAssistantSettingsRequest;
  using ::assistant::api::GetAssistantSettingsResponse;

  GetAssistantSettingsRequest request;
  *request.mutable_get_settings_ui_request()->mutable_selector() = selector;
  request.set_user_id(user_id);

  auto cb = base::BindOnce(
      [](base::OnceCallback<void(const GetAssistantSettingsResponse&)> on_done,
         const grpc::Status& status,
         const GetAssistantSettingsResponse& response) {
        LOG_GRPC_STATUS(/*level=*/2, status, "GetAssistantSettings")
        std::move(on_done).Run(response);
      },
      std::move(on_done));

  libassistant_client_->CallServiceMethod(request, std::move(cb),
                                          kDefaultStateConfig);
}

void AssistantClientImpl::SetLocaleOverride(const std::string& locale) {
  SetLocaleOverrideRequest request;
  request.set_locale(locale);

  libassistant_client_->CallServiceMethod(
      request, GetLoggingCallback<SetLocaleOverrideResponse>(__func__),
      kDefaultStateConfig);
}

std::string AssistantClientImpl::GetDeviceId() {
  return assistant_manager()->GetDeviceId();
}

void AssistantClientImpl::EnableListening(bool listening_enabled) {
  EnableListeningRequest request;
  request.set_enable(listening_enabled);

  libassistant_client_->CallServiceMethod(
      request, GetLoggingCallback<EnableListeningResponse>(__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::AddTimeToTimer(const std::string& id,
                                         const base::TimeDelta& duration) {
  ::assistant::api::AddTimeToTimerRequest request;
  request.set_timer_id(id);
  request.set_extra_time_seconds(duration.InSeconds());
  libassistant_client_->CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::AddTimeToTimerResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::PauseTimer(const std::string& timer_id) {
  ::assistant::api::PauseTimerRequest request;
  request.set_timer_id(timer_id);
  libassistant_client_->CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::PauseTimerResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::RemoveTimer(const std::string& timer_id) {
  ::assistant::api::RemoveTimerRequest request;
  request.set_timer_id(timer_id);
  libassistant_client_->CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::RemoveTimerResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::ResumeTimer(const std::string& timer_id) {
  ::assistant::api::ResumeTimerRequest request;
  request.set_timer_id(timer_id);
  libassistant_client_->CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::ResumeTimerResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::GetTimers(
    base::OnceCallback<void(const std::vector<assistant::AssistantTimer>&)>
        on_done) {
  ::assistant::api::GetTimersResponse response;

  libassistant_client_->CallServiceMethod(
      ::assistant::api::GetTimersRequest(),
      base::BindOnce(
          [](base::OnceCallback<void(
                 const std::vector<assistant::AssistantTimer>&)> on_done,
             const grpc::Status& status,
             const ::assistant::api::GetTimersResponse& response) {
            if (status.ok()) {
              std::move(on_done).Run(
                  ConstructAssistantTimersFromProto(response.timers()));
            } else {
              std::move(on_done).Run(/*timers=*/{});
            }
          },
          std::move(on_done)),
      kDefaultStateConfig);
}

void AssistantClientImpl::AddAlarmTimerEventObserver(
    GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
        observer) {
  grpc_services_.AddAlarmTimerEventObserver(observer);
}

// static
std::unique_ptr<AssistantClient> AssistantClient::Create(
    std::unique_ptr<assistant_client::AssistantManager> assistant_manager) {
  const bool is_chromeos_device = base::SysInfo::IsRunningOnChromeOS();
  return std::make_unique<AssistantClientImpl>(
      std::move(assistant_manager),
      GetLibassistantServiceAddress(is_chromeos_device),
      GetAssistantServiceAddress(is_chromeos_device));
}

}  // namespace ash::libassistant
