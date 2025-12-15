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

ExtensionInstallDecision::ExtensionInstallDecision() = default;
ExtensionInstallDecision::ExtensionInstallDecision(
    enterprise_management::ExtensionInstallPolicy::Action action,
    std::set<enterprise_management::ExtensionInstallPolicy::Reason> reasons)
    : action(action), reasons(reasons) {}
ExtensionInstallDecision::ExtensionInstallDecision(
    const ExtensionInstallDecision&) = default;
ExtensionInstallDecision::ExtensionInstallDecision(ExtensionInstallDecision&&) =
    default;

ExtensionInstallDecision::~ExtensionInstallDecision() = default;

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
    : policy_type_(policy_type), extra_param_(settings_entity_id) {}

CloudPolicyClientTypeParams::CloudPolicyClientTypeParams(
    const std::string& policy_type,
    ExtensionIdAndVersion extension_id_and_version)
    : CloudPolicyClientTypeParams(
          policy_type,
          base::BindRepeating(
              [](ExtensionIdAndVersion extension_id_and_version) {
                return std::set<ExtensionIdAndVersion>{
                    extension_id_and_version};
              },
              std::move(extension_id_and_version))) {}

CloudPolicyClientTypeParams::CloudPolicyClientTypeParams(
    const std::string& policy_type,
    ExtensionSetCallback extension_ids_and_version_getter)
    : policy_type_(policy_type),
      extra_param_(std::move(extension_ids_and_version_getter)) {
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
  if (policy_type_ != other.policy_type_) {
    return policy_type_ < other.policy_type_;
  }
  if (extra_param_.index() != other.extra_param_.index()) {
    return extra_param_.index() < other.extra_param_.index();
  }
  if (std::holds_alternative<std::string>(extra_param_) &&
      std::holds_alternative<std::string>(other.extra_param_)) {
    return std::get<std::string>(extra_param_) <
           std::get<std::string>(other.extra_param_);
  }

  if (std::holds_alternative<ExtensionSetCallback>(extra_param_) &&
      std::holds_alternative<ExtensionSetCallback>(other.extra_param_)) {
    return std::get<ExtensionSetCallback>(extra_param_).Run() <
           std::get<ExtensionSetCallback>(other.extra_param_).Run();
  }
  NOTREACHED() << "Unsupported extra param type";
}

bool CloudPolicyClientTypeParams::operator==(
    const CloudPolicyClientTypeParams& other) const {
  return std::tie(policy_type_, extra_param_) ==
         std::tie(other.policy_type_, other.extra_param_);
}

std::string CloudPolicyClientTypeParams::settings_entity_id() const {
  if (std::holds_alternative<std::string>(extra_param_)) {
    return std::get<std::string>(extra_param_);
  }
  return std::string();
}

std::set<ExtensionIdAndVersion>
CloudPolicyClientTypeParams::extension_ids_and_version() const {
  if (std::holds_alternative<ExtensionSetCallback>(extra_param_)) {
    return std::get<ExtensionSetCallback>(extra_param_).Run();
  }
  return std::set<ExtensionIdAndVersion>();
}
}  // namespace policy
