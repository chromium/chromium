// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_UTILS_SETTINGS_UTILS_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_UTILS_SETTINGS_UTILS_H_

#include "chromeos/assistant/internal/proto/shared/proto/v2/config_settings_interface.pb.h"

namespace assistant_client {
struct VoicelessResponse;
}  // namespace assistant_client

namespace ash::libassistant {

// Populates a |GetAssistantSettingsResponse| proto with corresponding fields
// from a |VoicelessResponse| struct.
::assistant::api::GetAssistantSettingsResponse ToGetSettingsResponseProto(
    const assistant_client::VoicelessResponse& voiceless_response);

// Populates an |UpdateAssistantSettingsResponse| proto with corresponding
// fields from a |VoicelessResponse| struct.
::assistant::api::UpdateAssistantSettingsResponse ToUpdateSettingsResponseProto(
    const assistant_client::VoicelessResponse& voiceless_response);

// Returns a serialized proto of |SettingsUi| by default (or a serialized proto
// of |GetSettingsUiResponse| if |include_header| is true) if the response
// status is ok, otherwise returns an empty string.
std::string UnwrapGetAssistantSettingsResponse(
    const ::assistant::api::GetAssistantSettingsResponse& response,
    bool include_header = false);

// Returns a serialized proto of |SettingsUiUpdateResult| if the response
// status is ok, otherwise returns an empty string.
std::string UnwrapUpdateAssistantSettingsResponse(
    const ::assistant::api::UpdateAssistantSettingsResponse& response);

}  // namespace ash::libassistant

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos::libassistant {
using ::ash::libassistant::UnwrapGetAssistantSettingsResponse;
using ::ash::libassistant::UnwrapUpdateAssistantSettingsResponse;
}  // namespace chromeos::libassistant
#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_UTILS_SETTINGS_UTILS_H_
