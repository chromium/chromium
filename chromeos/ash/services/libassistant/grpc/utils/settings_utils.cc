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

}  // namespace

std::string UnwrapGetAssistantSettingsResponse(
    const GetAssistantSettingsResponse& response,
    bool include_header) {
  const auto& response_details = response.response_details();
  if (response_details.has_status() &&
      response_details.status() ==
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
  }

  // TODO(xiaohuic): figure out how to log through libassistant.
  LOG(ERROR) << "GetAssistantSettings request failed.";
  if (response_details.has_status()) {
    LOG(ERROR) << "Failed with status: "
               << static_cast<int>(response_details.status());
  }
  if (response_details.has_error_message()) {
    LOG(ERROR) << "Error message: " << response_details.error_message();
  }

  // Upon failure, returns an empty string.
  return "";
}

std::string UnwrapUpdateAssistantSettingsResponse(
    const UpdateAssistantSettingsResponse& response) {
  const auto& response_details = response.response_details();
  if (response_details.has_status() &&
      response_details.status() ==
          ::assistant::api::ResponseDetails_Status_SUCCESS) {
    DCHECK(response.has_update_settings_ui_response() &&
           response.update_settings_ui_response().has_update_result());

    // Upon success, returns a serialized proto |SettingsUiUpdateResult|.
    return response.update_settings_ui_response()
        .update_result()
        .SerializeAsString();
  }

  // TODO(xiaohuic): figure out how to log through libassistant.
  LOG(ERROR) << "UpdateAssistantSettings request failed.";
  if (response_details.has_status()) {
    LOG(ERROR) << "Failed with status: "
               << static_cast<int>(response_details.status());
  }
  if (response_details.has_error_message()) {
    LOG(ERROR) << "ERROR message: " << response_details.error_message();
  }

  // Upon failure, returns an empty string.
  return "";
}

}  // namespace ash::libassistant
