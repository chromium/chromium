// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/external_services/event_handler_driver.h"

#include "chromeos/ash/services/libassistant/grpc/grpc_util.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/event_notification_interface.pb.h"

namespace ash::libassistant {

namespace {

constexpr char kAlarmTimerEventName[] = "AlarmTimerEvent";
constexpr char kAssistantDisplayEventName[] = "AssistantDisplayEvent";
constexpr char kConversationStateEventName[] = "ConversationStateEvent";
constexpr char kDeviceStateEventName[] = "DeviceStateEvent";
constexpr char kMediaActionFallbackEventName[] = "MediaActionFallbackEvent";
constexpr char kSpeakerIdEnrollmentEventName[] = "SpeakerIdEnrollmentEvent";
constexpr char kHandlerMethodName[] = "OnEventFromLibas";

template <typename EventSelection>
void PopulateRequest(const std::string& assistant_service_address,
                     const std::string& event_name,
                     ::assistant::api::RegisterEventHandlerRequest* request,
                     EventSelection* event_selection) {
  event_selection->set_select_all(true);
  auto* external_handler = request->mutable_handler();
  external_handler->set_server_address(assistant_service_address);
  external_handler->set_service_name(GetLibassistGrpcServiceName(event_name));
  external_handler->set_handler_method(kHandlerMethodName);
}

}  // namespace

template <>
::assistant::api::RegisterEventHandlerRequest
CreateRegistrationRequest<::assistant::api::AlarmTimerEventHandlerInterface>(
    const std::string& assistant_service_address) {
  ::assistant::api::RegisterEventHandlerRequest request;
  PopulateRequest(assistant_service_address, kAlarmTimerEventName, &request,
                  request.mutable_alarm_timer_events_to_handle());
  return request;
}

template <>
::assistant::api::RegisterEventHandlerRequest CreateRegistrationRequest<
    ::assistant::api::AssistantDisplayEventHandlerInterface>(
    const std::string& assistant_service_address) {
  ::assistant::api::RegisterEventHandlerRequest request;
  PopulateRequest(assistant_service_address, kAssistantDisplayEventName,
                  &request,
                  request.mutable_assistant_display_events_to_handle());
  return request;
}

template <>
::assistant::api::RegisterEventHandlerRequest CreateRegistrationRequest<
    ::assistant::api::ConversationStateEventHandlerInterface>(
    const std::string& assistant_service_address) {
  ::assistant::api::RegisterEventHandlerRequest request;
  PopulateRequest(assistant_service_address, kConversationStateEventName,
                  &request,
                  request.mutable_conversation_state_events_to_handle());
  return request;
}

template <>
::assistant::api::RegisterEventHandlerRequest
CreateRegistrationRequest<::assistant::api::DeviceStateEventHandlerInterface>(
    const std::string& assistant_service_address) {
  ::assistant::api::RegisterEventHandlerRequest request;
  PopulateRequest(assistant_service_address, kDeviceStateEventName, &request,
                  request.mutable_device_state_events_to_handle());
  return request;
}

template <>
::assistant::api::RegisterEventHandlerRequest CreateRegistrationRequest<
    ::assistant::api::MediaActionFallbackEventHandlerInterface>(
    const std::string& assistant_service_address) {
  ::assistant::api::RegisterEventHandlerRequest request;
  PopulateRequest(assistant_service_address, kMediaActionFallbackEventName,
                  &request,
                  request.mutable_media_action_fallback_events_to_handle());
  return request;
}

template <>
::assistant::api::RegisterEventHandlerRequest CreateRegistrationRequest<
    ::assistant::api::SpeakerIdEnrollmentEventHandlerInterface>(
    const std::string& assistant_service_address) {
  ::assistant::api::RegisterEventHandlerRequest request;
  PopulateRequest(assistant_service_address, kSpeakerIdEnrollmentEventName,
                  &request,
                  request.mutable_speaker_id_enrollment_events_to_handle());
  return request;
}

}  // namespace ash::libassistant
