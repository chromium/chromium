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

#include "base/functional/overloaded.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "chrome/enterprise_companion/proto/enterprise_companion_event.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"

namespace enterprise_companion {

// Errors defined by the enterprise companion app. Ordinals are transmitted
// across IPC and network boundaries. Entries must not be removed or reordered.
enum class ApplicationError {
  // An action failed due to the client not being registered.
  kRegistrationPreconditionFailed,
  // DMStorage reports that it is not capable of persisting policies.
  kPolicyPersistenceImpossible,
  // DMStorage failed to persist the policies.
  kPolicyPersistenceFailed,
  // The global singleton lock could not be acquired.
  kCannotAcquireLock,
  // An IPC connection could not be established.
  kMojoConnectionFailed,
  // Installation or uninstallation failed.
  kInstallationFailed,
  // The IPC caller is not allowed to perform the requested action.
  kIpcCallerNotAllowed,
  // Failed to initialize COM on Windows.
  kCOMInitializationFailed,
};

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
        base::Overloaded{[](std::monostate) { return 0; },
                         [](const PersistedError& error) { return error.code; },
                         [](auto&& x) { return static_cast<int>(x); }},
        status_variant_);
  }

  std::string description() const;

  mojom::StatusPtr ToMojomStatus() const {
    return mojom::Status::New(space(), code(), description());
  }

  proto::Status ToProtoStatus() const {
    proto::Status status;
    status.set_space(space());
    status.set_code(code());
    return status;
  }

  static EnterpriseCompanionStatus FromMojomStatus(mojom::StatusPtr status) {
    return status->space == 0
               ? Success()
               : EnterpriseCompanionStatus(StatusVariant(PersistedError(
                     status->space, status->code, status->description)));
  }

  static EnterpriseCompanionStatus FromProtoStatus(
      const proto::Status& status) {
    return status.space() == 0
               ? Success()
               : EnterpriseCompanionStatus(StatusVariant(PersistedError(
                     status.space(), status.code(), "<missing description>")));
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
  StatusVariant status_variant_;

  explicit EnterpriseCompanionStatus(StatusVariant status_variant);

  template <size_t I, typename T>
  static EnterpriseCompanionStatus From(T&& status) {
    return EnterpriseCompanionStatus(
        StatusVariant(std::in_place_index_t<I>(), std::forward<T>(status)));
  }
};

// A general-purpose callback type for operations that produce an
// EnterpriseCompanionStatus.
using StatusCallback =
    base::OnceCallback<void(const EnterpriseCompanionStatus&)>;

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_STATUS_H_
