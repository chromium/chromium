// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_UTIL_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_UTIL_H_

#include <optional>
#include <string>
#include <vector>

namespace ash::assistant {
struct AndroidAppInfo;
}

namespace assistant {
namespace api {
class Interaction;
}  // namespace api
}  // namespace assistant

namespace base {
class FilePath;
}  // namespace base

namespace chromeos::assistant {
struct DeviceSetting;
}

namespace ash::libassistant {

// Creates the configuration for libassistant.
std::string CreateLibAssistantConfig(
    std::optional<std::string> s3_server_uri_override,
    std::optional<std::string> device_id_override);

// Returns the path where all downloaded LibAssistant resources are stored.
base::FilePath GetBaseAssistantDir();

::assistant::api::Interaction CreateVerifyProviderResponseInteraction(
    const int interaction_id,
    const std::vector<assistant::AndroidAppInfo>& apps_info);

::assistant::api::Interaction CreateGetDeviceSettingInteraction(
    int interaction_id,
    const std::vector<chromeos::assistant::DeviceSetting>& device_settings);

// `action_index` is the index of the actions and buttons.
::assistant::api::Interaction CreateNotificationRequestInteraction(
    const std::string& notification_id,
    const std::string& consistent_token,
    const std::string& opaque_token,
    const int action_index);

// `grouping_keys` are the keys to group multiple notifications together.
::assistant::api::Interaction CreateNotificationDismissedInteraction(
    const std::string& notification_id,
    const std::string& consistent_token,
    const std::string& opaque_token,
    const std::vector<std::string>& grouping_keys);

::assistant::api::Interaction CreateEditReminderInteraction(
    const std::string& reminder_id);

::assistant::api::Interaction CreateOpenProviderResponseInteraction(
    const int interaction_id,
    const bool provider_found);

::assistant::api::Interaction CreateSendFeedbackInteraction(
    bool assistant_debug_info_allowed,
    const std::string& feedback_description,
    const std::string& screenshot_png = std::string());

::assistant::api::Interaction CreateTextQueryInteraction(
    const std::string& query);

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_UTIL_H_
