// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_WEBUI_JSON_GENERATION_H_
#define COMPONENTS_POLICY_CORE_BROWSER_WEBUI_JSON_GENERATION_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/policy/policy_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}

namespace policy {
class PolicyConversionsClient;

POLICY_EXPORT extern const char kChromeMetadataVersionKey[];
POLICY_EXPORT extern const char kChromeMetadataOSKey[];
POLICY_EXPORT extern const char kChromeMetadataPlatformKey[];
POLICY_EXPORT extern const char kChromeMetadataRevisionKey[];

// Simple object containing parameters used to generate a string of JSON from
// a set of policies.
struct POLICY_EXPORT JsonGenerationParams {
  explicit JsonGenerationParams();
  ~JsonGenerationParams();

  JsonGenerationParams(JsonGenerationParams&&);

  JsonGenerationParams& with_application_name(
      const std::string& other_application_name) {
    application_name = other_application_name;
    return *this;
  }

  JsonGenerationParams& with_channel_name(
      const std::string& other_channel_name) {
    channel_name = other_channel_name;
    return *this;
  }

  JsonGenerationParams& with_processor_variation(
      const std::string& other_processor_variation) {
    processor_variation = other_processor_variation;
    return *this;
  }

  JsonGenerationParams& with_cohort_name(const std::string& other_cohort_name) {
    cohort_name = other_cohort_name;
    return *this;
  }

  JsonGenerationParams& with_os_name(const std::string& other_os_name) {
    os_name = other_os_name;
    return *this;
  }

  JsonGenerationParams& with_platform_name(
      const std::string& other_platform_name) {
    platform_name = other_platform_name;
    return *this;
  }

  std::string application_name;
  std::string channel_name;
  std::string processor_variation;
  absl::optional<std::string> cohort_name;
  absl::optional<std::string> os_name;
  absl::optional<std::string> platform_name;
};

// Generates a string of JSON containing the currently applied policies along
// with additional metadata about the current device/build, based both on what
// is stored in |params| and also information that is statically available.
POLICY_EXPORT std::string GenerateJson(
    std::unique_ptr<PolicyConversionsClient> client,
    base::Value status,
    const JsonGenerationParams& params);

// Returns metadata about the current device/build, based both on what
// is stored in |params| and also information that is statically available.
POLICY_EXPORT base::Value::Dict GetChromeMetadataValue(
    const JsonGenerationParams& params);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_WEBUI_JSON_GENERATION_H_
