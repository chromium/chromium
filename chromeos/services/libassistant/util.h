// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_UTIL_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_UTIL_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace assistant {
namespace api {
class Interaction;
}  // namespace api
}  // namespace assistant

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace assistant {
struct AndroidAppInfo;
struct DeviceSetting;
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

// Creates the configuration for libassistant.
std::string CreateLibAssistantConfig(
    absl::optional<std::string> s3_server_uri_override,
    absl::optional<std::string> device_id_override);

// Returns the path where all downloaded LibAssistant resources are stored.
base::FilePath GetBaseAssistantDir();

::assistant::api::Interaction CreateVerifyProviderResponseInteraction(
    const int interaction_id,
    const std::vector<chromeos::assistant::AndroidAppInfo>& apps_info);

::assistant::api::Interaction CreateGetDeviceSettingInteraction(
    int interaction_id,
    const std::vector<chromeos::assistant::DeviceSetting>& device_settings);

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_UTIL_H_
