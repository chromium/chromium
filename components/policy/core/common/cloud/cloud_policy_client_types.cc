// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_client_types.h"

#include "base/functional/callback.h"
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

CloudPolicyClientTypeParams::CloudPolicyClientTypeParams(
    const std::string& policy_type,
    const std::string& settings_entity_id)
    : policy_type_(policy_type), settings_entity_id_(settings_entity_id) {}

CloudPolicyClientTypeParams::CloudPolicyClientTypeParams(
    const std::string& policy_type,
    ExtensionIdAndVersion extension_id_and_version)
    : policy_type_(policy_type),
      settings_entity_id_(base::StringPrintf(
          "%s@%s",
          extension_id_and_version.extension_id.c_str(),
          extension_id_and_version.extension_version.c_str())),
      extension_ids_and_version_getter_(base::BindRepeating(
          [](ExtensionIdAndVersion extension_id_and_version) {
            return std::set<ExtensionIdAndVersion>{extension_id_and_version};
          },
          std::move(extension_id_and_version))) {
  CHECK(IsExtensionInstallPolicyType(policy_type));
}

CloudPolicyClientTypeParams::CloudPolicyClientTypeParams(
    const std::string& policy_type,
    base::RepeatingCallback<std::set<ExtensionIdAndVersion>()>
        extension_ids_and_version_getter)
    : policy_type_(policy_type),
      // settings_entity_id_ is intentionally left empty as it's expected to be
      // derived or not applicable when using a getter for multiple extensions.
      extension_ids_and_version_getter_(
          std::move(extension_ids_and_version_getter)) {
  CHECK(IsExtensionInstallPolicyType(policy_type));
}

CloudPolicyClientTypeParams::CloudPolicyClientTypeParams(
    const CloudPolicyClientTypeParams&) = default;

CloudPolicyClientTypeParams::CloudPolicyClientTypeParams(
    CloudPolicyClientTypeParams&&) = default;

CloudPolicyClientTypeParams::~CloudPolicyClientTypeParams() = default;

CloudPolicyClientTypeParams& CloudPolicyClientTypeParams::operator=(
    const CloudPolicyClientTypeParams&) = default;

CloudPolicyClientTypeParams& CloudPolicyClientTypeParams::operator=(
    CloudPolicyClientTypeParams&&) = default;

bool CloudPolicyClientTypeParams::operator<(
    const CloudPolicyClientTypeParams& other) const {
  return std::tie(policy_type_, settings_entity_id_) <
         std::tie(other.policy_type_, other.settings_entity_id_);
}

bool CloudPolicyClientTypeParams::operator==(
    const CloudPolicyClientTypeParams& other) const {
  return std::tie(policy_type_, settings_entity_id_) ==
         std::tie(other.policy_type_, other.settings_entity_id_);
}

std::set<ExtensionIdAndVersion>
CloudPolicyClientTypeParams::extension_ids_and_version() const {
  return extension_ids_and_version_getter_
             ? extension_ids_and_version_getter_.Run()
             : std::set<ExtensionIdAndVersion>();
}
}  // namespace policy
