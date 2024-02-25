// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/grpc_libassistant_client.h"

#include <memory>

#include "base/check.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_util.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/alarm_timer_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/audio_utils_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/bootup_settings_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/config_settings_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/customer_registration_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/action_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/display_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/event_notification_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/experiment_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/speaker_id_enrollment_interface.pb.h"

namespace ash::libassistant {

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::RegisterCustomerRequest>() {
  // CustomerRegistrationService handles CustomerRegistrationRequest sent from
  // libassistant customers to register themselves before allowing to use
  // libassistant services.
  return GetLibassistGrpcMethodName("CustomerRegistrationService",
                                    "RegisterCustomer");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::UpdateExperimentIdsRequest>() {
  // ExperimentService.
  return GetLibassistGrpcMethodName("ExperimentService", "UpdateExperimentIds");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::RegisterEventHandlerRequest>() {
  // EventNotificationService handles RegisterEventHandler sent from
  // libassistant customers to register themselves for events.
  return GetLibassistGrpcMethodName("EventNotificationService",
                                    "RegisterEventHandler");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::ResetAllDataAndShutdownRequest>() {
  // ConfigSettingsService.
  return GetLibassistGrpcMethodName("ConfigSettingsService",
                                    "ResetAllDataAndShutdown");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::OnDisplayRequestRequest>() {
  // DisplayService handles display requests sent from libassistant customers.
  return GetLibassistGrpcMethodName("DisplayService", "OnDisplayRequest");
}

template <>
std::string GetLibassistGrpcMethodName<::assistant::api::SendQueryRequest>() {
  // QueryService handles queries sent from libassistant customers.
  return GetLibassistGrpcMethodName(chromeos::assistant::kQueryServiceName,
                                    chromeos::assistant::kSendQueryMethodName);
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::RegisterActionModuleRequest>() {
  // QueryService handles RegisterActionModule sent from
  // libassistant customers to register themselves to handle actions.
  return GetLibassistGrpcMethodName(
      chromeos::assistant::kQueryServiceName,
      chromeos::assistant::kRegisterActionModuleMethodName);
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::StartVoiceQueryRequest>() {
  return GetLibassistGrpcMethodName(
      chromeos::assistant::kQueryServiceName,
      chromeos::assistant::kStartVoiceQueryMethodName);
}

template <>
std::string GetLibassistGrpcMethodName<::assistant::api::StopQueryRequest>() {
  return GetLibassistGrpcMethodName(chromeos::assistant::kQueryServiceName,
                                    chromeos::assistant::kStopQueryMethodName);
}

template <>
std::string GetLibassistGrpcMethodName<::assistant::api::SetAuthInfoRequest>() {
  // Returns method used for sending authentication information to Libassistant.
  // Can be called during or after bootup is completed.
  return GetLibassistGrpcMethodName("BootupSettingsService", "SetAuthInfo");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::SetInternalOptionsRequest>() {
  // Return method used for sending internal options to Libassistant. Can be
  // called during or after bootup is completed.
  return GetLibassistGrpcMethodName("BootupSettingsService",
                                    "SetInternalOptions");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::UpdateAssistantSettingsRequest>() {
  // Updates assistant settings on the server.
  return GetLibassistGrpcMethodName("ConfigSettingsService",
                                    "UpdateAssistantSettings");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::GetAssistantSettingsRequest>() {
  // Returns assistant settings from the server.
  return GetLibassistGrpcMethodName("ConfigSettingsService",
                                    "GetAssistantSettings");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::SetLocaleOverrideRequest>() {
  // Set the locale override of the device. This will override the locale
  // obtained from user's assistant settings.
  return GetLibassistGrpcMethodName("ConfigSettingsService",
                                    "SetLocaleOverride");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::EnableListeningRequest>() {
  // Enables or disables Assistant listening on the device.
  return GetLibassistGrpcMethodName("AudioUtilsService", "EnableListening");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::AddTimeToTimerRequest>() {
  return GetLibassistGrpcMethodName("AlarmTimerService", "AddTimeToTimer");
}

template <>
std::string GetLibassistGrpcMethodName<::assistant::api::PauseTimerRequest>() {
  return GetLibassistGrpcMethodName("AlarmTimerService", "PauseTimer");
}

template <>
std::string GetLibassistGrpcMethodName<::assistant::api::RemoveTimerRequest>() {
  return GetLibassistGrpcMethodName("AlarmTimerService", "RemoveTimer");
}

template <>
std::string GetLibassistGrpcMethodName<::assistant::api::ResumeTimerRequest>() {
  return GetLibassistGrpcMethodName("AlarmTimerService", "ResumeTimer");
}

template <>
std::string GetLibassistGrpcMethodName<::assistant::api::GetTimersRequest>() {
  return GetLibassistGrpcMethodName("AlarmTimerService", "GetTimers");
}

template <>
std::string GetLibassistGrpcMethodName<
    ::assistant::api::StartSpeakerIdEnrollmentRequest>() {
  return GetLibassistGrpcMethodName("SpeakerIdEnrollmentService",
                                    "StartSpeakerIdEnrollment");
}

template <>
std::string GetLibassistGrpcMethodName<
    ::assistant::api::CancelSpeakerIdEnrollmentRequest>() {
  return GetLibassistGrpcMethodName("SpeakerIdEnrollmentService",
                                    "CancelSpeakerIdEnrollment");
}

template <>
std::string GetLibassistGrpcMethodName<
    ::assistant::api::GetSpeakerIdEnrollmentInfoRequest>() {
  return GetLibassistGrpcMethodName("SpeakerIdEnrollmentService",
                                    "GetSpeakerIdEnrollmentInfo");
}

GrpcLibassistantClient::GrpcLibassistantClient(
    std::shared_ptr<grpc::Channel> channel)
    : channel_(std::move(channel)), client_thread_("gRPCLibassistantClient") {
  DCHECK(channel_);
}

GrpcLibassistantClient::~GrpcLibassistantClient() = default;

}  // namespace ash::libassistant
