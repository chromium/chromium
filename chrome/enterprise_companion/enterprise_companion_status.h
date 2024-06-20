// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_STATUS_H_
#define CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_STATUS_H_

#include <utility>
#include <variant>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"

namespace enterprise_companion {

// Canonical view of statuses used across the application.
class EnterpriseCompanionStatus {
 public:
  using Ok = std::monostate;
  using StatusVariant = std::variant<Ok,
                                     policy::DeviceManagementStatus,
                                     policy::CloudPolicyValidatorBase::Status>;
  EnterpriseCompanionStatus() = delete;

  // Constructs an `Ok` status.
  static EnterpriseCompanionStatus Success() {
    return EnterpriseCompanionStatus(Ok());
  }

  bool ok() const { return std::holds_alternative<Ok>(status_variant_); }

  auto operator<=>(const EnterpriseCompanionStatus& other) const = default;

  // policy::DeviceManagementStatus:
  static EnterpriseCompanionStatus FromDeviceManagementStatus(
      policy::DeviceManagementStatus status) {
    return status == policy::DM_STATUS_SUCCESS ? Success() : From<1>(status);
  }
  bool EqualsDeviceManagementStatus(
      policy::DeviceManagementStatus other) const {
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
      policy::CloudPolicyValidatorBase::Status other) const {
    return operator==(From<2>(other));
  }

 private:
  explicit EnterpriseCompanionStatus(StatusVariant&& status_variant)
      : status_variant_(std::move(status_variant)) {}

  template <size_t I, typename T>
  static EnterpriseCompanionStatus From(T&& status) {
    return EnterpriseCompanionStatus(
        StatusVariant(std::in_place_index_t<I>(), std::forward<T>(status)));
  }

  StatusVariant status_variant_;
};

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_STATUS_H_
