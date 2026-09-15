// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_client_types.h"

#include <tuple>

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

namespace {

bool IsExtensionInstallPolicyType(const std::string& policy_type) {
  return policy_type ==
             dm_protocol::kChromeExtensionInstallUserCloudPolicyType ||
         policy_type ==
             dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType;
}

}  // namespace

bool ExtensionIdAndVersion::operator<(
    const ExtensionIdAndVersion& other) const {
  return std::tie(extension_id, extension_version) <
         std::tie(other.extension_id, other.extension_version);
}

bool ExtensionIdAndVersion::operator==(
    const ExtensionIdAndVersion& other) const {
  return std::tie(extension_id, extension_version) ==
         std::tie(other.extension_id, other.extension_version);
}

std::string ExtensionIdAndVersion::ToString() const {
  return base::StringPrintf("%s@%s", extension_id.c_str(),
                            extension_version.c_str());
}

PolicyTypeToFetch::PolicyTypeToFetch(const std::string& policy_type,
                                     const std::string& settings_entity_id)
    : policy_type_(policy_type), extra_param_(settings_entity_id) {}

PolicyTypeToFetch::PolicyTypeToFetch(const std::string& policy_type,
                                     ExtensionsProvider* extension_set_provider)
    : policy_type_(policy_type),
      extra_param_(
          raw_ref<ExtensionsProvider>::from_ptr(extension_set_provider)) {
  CHECK(IsExtensionInstallPolicyType(policy_type));
}

PolicyTypeToFetch::PolicyTypeToFetch(const PolicyTypeToFetch&) = default;

PolicyTypeToFetch::PolicyTypeToFetch(PolicyTypeToFetch&&) = default;

PolicyTypeToFetch::~PolicyTypeToFetch() = default;

PolicyTypeToFetch& PolicyTypeToFetch::operator=(const PolicyTypeToFetch&) =
    default;

PolicyTypeToFetch& PolicyTypeToFetch::operator=(PolicyTypeToFetch&&) = default;

std::string PolicyTypeToFetch::settings_entity_id() const {
  if (std::holds_alternative<std::string>(extra_param_)) {
    return std::get<std::string>(extra_param_);
  }
  CHECK(std::holds_alternative<ExtensionsProviderRef>(extra_param_));
  return std::string();
}

std::set<ExtensionIdAndVersion> PolicyTypeToFetch::extension_ids_and_version()
    const {
  if (std::holds_alternative<ExtensionsProviderRef>(extra_param_)) {
    return std::get<ExtensionsProviderRef>(extra_param_)->GetExtensions();
  }
  return std::set<ExtensionIdAndVersion>();
}

}  // namespace policy
