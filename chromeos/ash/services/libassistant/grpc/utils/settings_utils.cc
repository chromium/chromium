// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/utils/settings_utils.h"

#include "base/check.h"
#include "base/logging.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/settings_ui.pb.h"

namespace ash::libassistant {

namespace {

using ::assistant::api::GetAssistantSettingsResponse;
using ::assistant::api::ResponseDetails;
using ::assistant::api::UpdateAssistantSettingsResponse;
using assistant_client::VoicelessResponse;

}  // namespace

GetAssistantSettingsResponse ToGetSettingsResponseProto(
    const assistant_client::VoicelessResponse& voiceless_response) {
  GetAssistantSettingsResponse proto;
  auto* details = proto.mutable_response_details();
  int status = static_cast<int>(voiceless_response.status);
  if (ResponseDetails::Status_IsValid(status))
    details->set_status(static_cast<ResponseDetails::Status>(status));

  // Either a serialized |GetSettingsUiResponse| proto or an error message will
  // be filled in upon success or failure of the GetAssistantSettingsRequest.
  switch (voiceless_response.status) {
    case VoicelessResponse::Status::SUCCESS:
      proto.mutable_get_settings_ui_response()->ParseFromString(
          voiceless_response.response_proto);
      break;
    case VoicelessResponse::Status::COMMUNICATION_ERROR:
    case VoicelessResponse::Status::NO_RESPONSE_ERROR:
    case VoicelessResponse::Status::DESERIALIZATION_ERROR:
    case VoicelessResponse::Status::S3_ERROR:
      // Sets the error message if the status falls in the category of failure.
      details->set_error_message(voiceless_response.error_message);
      break;
  }

  return proto;
}

UpdateAssistantSettingsResponse ToUpdateSettingsResponseProto(
    const VoicelessResponse& voiceless_response) {
  UpdateAssistantSettingsResponse proto;
  auto* details = proto.mutable_response_details();
  int status = static_cast<int>(voiceless_response.status);
  if (ResponseDetails::Status_IsValid(status)) {
    // We assume that VoicelessResponse::Status and ResponseDetails::Status is
    // always synced.
    details->set_status(static_cast<ResponseDetails::Status>(status));
  }

  // Either a serialized |UpdateSettingsUiResponse| proto or an error message
  // will be filled in upon success or failure of the
  // UpdateAssistantSettingsRequest.
  switch (voiceless_response.status) {
    case VoicelessResponse::Status::SUCCESS:
      proto.mutable_update_settings_ui_response()->ParseFromString(
          voiceless_response.response_proto);
      break;
    case VoicelessResponse::Status::COMMUNICATION_ERROR:
    case VoicelessResponse::Status::NO_RESPONSE_ERROR:
    case VoicelessResponse::Status::DESERIALIZATION_ERROR:
    case VoicelessResponse::Status::S3_ERROR:
      // Sets the error message if the status falls in the category of failure.
      details->set_error_message(voiceless_response.error_message);
      break;
  }

  return proto;
}

std::string UnwrapGetAssistantSettingsResponse(
    const GetAssistantSettingsResponse& response,
    bool include_header) {
  const auto& response_details = response.response_details();
  DCHECK(response_details.has_status());

  if (response_details.status() ==
      ::assistant::api::ResponseDetails_Status_SUCCESS) {
    DCHECK(response.has_get_settings_ui_response() &&
           response.get_settings_ui_response().has_settings());

    // Upon success, returns a serialized proto |SettingsUi|
    // or |GetSettingsUiResponse| based on the value of |include_header|.
    return include_header
               ? response.get_settings_ui_response().SerializeAsString()
               : response.get_settings_ui_response()
                     .settings()
                     .SerializeAsString();
  } else {
    // TODO(xiaohuic): figure out how to log through libassistant.
    LOG(ERROR) << "Get settings request error with status: "
               << static_cast<int>(response_details.status());
    LOG(ERROR) << "Error message: " << response_details.error_message();

    // Upon failure, returns an empty string.
    return "";
  }
}

std::string UnwrapUpdateAssistantSettingsResponse(
    const UpdateAssistantSettingsResponse& response) {
  const auto& response_details = response.response_details();
  DCHECK(response_details.has_status());

  if (response_details.status() ==
      ::assistant::api::ResponseDetails_Status_SUCCESS) {
    DCHECK(response.has_update_settings_ui_response() &&
           response.update_settings_ui_response().has_update_result());

    // Upon success, returns a serialized proto |SettingsUiUpdateResult|.
    return response.update_settings_ui_response()
        .update_result()
        .SerializeAsString();
  } else {
    // TODO(xiaohuic): figure out how to log through libassistant.
    DCHECK(response_details.has_error_message());
    LOG(ERROR) << "UpdateAssistantSettings request failed with status: "
               << static_cast<int>(response_details.status());
    LOG(ERROR) << "ERROR message: " << response_details.error_message();

    // Upon failure, returns an empty string.
    return "";
  }
}

}  // namespace ash::libassistant
