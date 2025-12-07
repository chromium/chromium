// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_STATUS_H_
#define CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_STATUS_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <variant>

#include "chrome/enterprise_companion/constants.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace enterprise_companion {

// Represents an error which was deserialized from an external source (e.g.
// across an IPC or network boundary). The error may not be known to this
// version of the application.
struct PersistedError {
  PersistedError(int space, int code, const std::string& description);
  PersistedError(const PersistedError&);
  PersistedError(PersistedError&&);
  ~PersistedError();

  PersistedError& operator=(const PersistedError&);
  PersistedError& operator=(PersistedError&&);

  // The error space, as defined by `EnterpriseCompanionStatus`.
  int space;
  // The error code, as defined by `EnterpriseCompanionStatus`.
  int code;
  // A human readable description of the error.
  std::string description;

  auto operator<=>(const PersistedError& other) const = default;
};

// Canonical view of statuses used across the application.
class EnterpriseCompanionStatus {
 public:
  using Ok = std::monostate;
  using PosixErrno = int;
  // The indices of the underlying variant are used by this implementation and
  // are transmitted across RPC boundaries. Existing entries should not be
  // reordered.
  using StatusVariant = std::variant<Ok,
                                     policy::DeviceManagementStatus,
                                     policy::CloudPolicyValidatorBase::Status,
                                     ApplicationError,
                                     PersistedError,
                                     PosixErrno>;
  EnterpriseCompanionStatus() = delete;
  EnterpriseCompanionStatus(const EnterpriseCompanionStatus&);
  ~EnterpriseCompanionStatus();

  // Constructs an `Ok` status.
  static EnterpriseCompanionStatus Success() {
    return EnterpriseCompanionStatus(Ok());
  }

  bool ok() const { return std::holds_alternative<Ok>(status_variant_); }

  // Indicates the status space for the code. As an implementation detail, the
  // space is the index of the code's type within `StatusVariant`.
  int space() const {
    if (const PersistedError* deserialized_error =
            std::get_if<PersistedError>(&status_variant_)) {
      return deserialized_error->space;
    } else {
      return status_variant_.index();
    }
  }

  int code() const {
    return std::visit(
        absl::Overload{[](std::monostate) { return 0; },
                       [](const PersistedError& error) { return error.code; },
                       [](auto&& x) { return static_cast<int>(x); }},
        status_variant_);
  }

  std::string description() const;

  static EnterpriseCompanionStatus FromPersistedError(PersistedError error) {
    return error.space == 0 ? Success()
                            : EnterpriseCompanionStatus(StatusVariant(error));
  }

  mojom::StatusPtr ToMojomStatus() const {
    return mojom::Status::New(space(), code(), description());
  }

  static EnterpriseCompanionStatus FromMojomStatus(mojom::StatusPtr status) {
    return FromPersistedError(
        PersistedError(status->space, status->code, status->description));
  }

  bool operator==(const EnterpriseCompanionStatus& other) const {
    return space() == other.space() && code() == other.code();
  }

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

  // ApplicationError:
  explicit EnterpriseCompanionStatus(ApplicationError error);
  bool EqualsApplicationError(ApplicationError other) const {
    return operator==(From<3>(other));
  }

  // PosixErrno:
  static EnterpriseCompanionStatus FromPosixErrno(PosixErrno error) {
    return From<5>(error);
  }
  bool EqualsPosixErrno(PosixErrno other) const {
    return operator==(From<5>(other));
  }

 private:
  explicit EnterpriseCompanionStatus(StatusVariant status_variant);

  template <size_t I, typename T>
  static EnterpriseCompanionStatus From(T&& status) {
    return EnterpriseCompanionStatus(
        StatusVariant(std::in_place_index_t<I>(), std::forward<T>(status)));
  }

  StatusVariant status_variant_;
};

// A general-purpose callback type for operations that produce an
// EnterpriseCompanionStatus.
using StatusCallback =
    base::OnceCallback<void(const EnterpriseCompanionStatus&)>;

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_STATUS_H_
