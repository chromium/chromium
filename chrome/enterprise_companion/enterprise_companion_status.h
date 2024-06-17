// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_STATUS_H_
#define CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_STATUS_H_

#include <utility>
#include <variant>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace enterprise_companion {

// Canonical view of statuses used across the application.
class EnterpriseCompanionStatus {
 public:
  using Ok = std::monostate;
  using StatusVariant = std::variant<Ok,
                                     policy::DeviceManagementStatus,
                                     policy::CloudPolicyValidatorBase::Status>;
  EnterpriseCompanionStatus() = delete;

  bool ok() const { return std::holds_alternative<Ok>(status_variant_); }

  auto operator<=>(const EnterpriseCompanionStatus& other) const = default;

  // Constructs an `Ok` status.
  static EnterpriseCompanionStatus Success() {
    return EnterpriseCompanionStatus(Ok());
  }

  // policy::DeviceManagementStatus:
  static EnterpriseCompanionStatus FromDeviceManagementStatus(
      policy::DeviceManagementStatus status) {
    return status == policy::DM_STATUS_SUCCESS ? Success() : From<1>(status);
  }
  bool EqualsDeviceManagementStatus(
      const policy::DeviceManagementStatus& other) {
    return operator==(From<1>(other));
  }

  // policy::CloudPolicyValidatorBase::Status:
  static EnterpriseCompanionStatus FromCloudPolicyValidationResult(
      policy::CloudPolicyValidatorBase::Status status) {
    return status == policy::CloudPolicyValidatorBase::VALIDATION_OK
               ? Success()
               : From<2>(status);
  }
  bool EqualsCloudPolicyValidationResult(
      const policy::CloudPolicyValidatorBase::Status& other) {
    return operator==(From<2>(other));
  }

 private:
  StatusVariant status_variant_;

  template <size_t I, typename T>
  static EnterpriseCompanionStatus From(T&& status) {
    return EnterpriseCompanionStatus(
        StatusVariant(std::in_place_index_t<I>(), std::forward<T>(status)));
  }

  explicit EnterpriseCompanionStatus(StatusVariant&& status_variant)
      : status_variant_(std::move(status_variant)) {}
};

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_STATUS_H_
