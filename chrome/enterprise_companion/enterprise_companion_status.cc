// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion_status.h"

#include <string>
#include <variant>

#include "base/functional/overloaded.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"

#if BUILDFLAG(IS_POSIX)
#include "base/posix/safe_strerror.h"
#else
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#endif

namespace enterprise_companion {

namespace {

constexpr std::string DeviceManagementStatusToString(
    policy::DeviceManagementStatus status) {
  switch (status) {
    case policy::DM_STATUS_SUCCESS:
      return "Success";
    case policy::DM_STATUS_REQUEST_INVALID:
      return "Request payload invalid";
    case policy::DM_STATUS_REQUEST_FAILED:
      return "HTTP request failed";
    case policy::DM_STATUS_TEMPORARY_UNAVAILABLE:
      return "Server temporarily unavailable";
    case policy::DM_STATUS_HTTP_STATUS_ERROR:
      return "HTTP request returned a non-success code";
    case policy::DM_STATUS_RESPONSE_DECODING_ERROR:
      return "Response could not be decoded";
    case policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
      return "Management not supported";
    case policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
      return "Device not found";
    case policy::DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
      return "Device token invalid";
    case policy::DM_STATUS_SERVICE_ACTIVATION_PENDING:
      return "Activation pending";
    case policy::DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
      return "The serial number is not valid or not known to the server";
    case policy::DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
      return "The device id used for registration is already taken";
    case policy::DM_STATUS_SERVICE_MISSING_LICENSES:
      return "The licenses have expired or have been exhausted";
    case policy::DM_STATUS_SERVICE_DEPROVISIONED:
      return "The administrator has deprovisioned this client";
    case policy::DM_STATUS_SERVICE_DOMAIN_MISMATCH:
      return "Device registration for the wrong domain";
    case policy::DM_STATUS_CANNOT_SIGN_REQUEST:
      return "Request could not be signed";
    case policy::DM_STATUS_REQUEST_TOO_LARGE:
      return "Request body is too large";
    case policy::DM_STATUS_SERVICE_TOO_MANY_REQUESTS:
      return "Too many requests";
    case policy::DM_STATUS_SERVICE_DEVICE_NEEDS_RESET:
      return "The device needs to be reset";
    case policy::DM_STATUS_SERVICE_POLICY_NOT_FOUND:
      return "Policy not found";
    case policy::DM_STATUS_SERVICE_ARC_DISABLED:
      return "ARC is not enabled on this domain";
    case policy::DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE:
      return "Non-dasher account with packaged license can't enroll";
    case policy::DM_STATUS_SERVICE_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL:
      return "Not eligible enterprise account can't enroll";
    case policy::DM_STATUS_SERVICE_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED:
      return "Enterprise TOS has not been accepted";
    case policy::DM_STATUS_SERVICE_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE:
      return "Illegal account for packaged EDU license";
    case policy::DM_STATUS_SERVICE_INVALID_PACKAGED_DEVICE_FOR_KIOSK:
      return "Packaged license device can't enroll KIOSK";
  }
}

constexpr std::string ApplicationErrorToString(ApplicationError error) {
  switch (error) {
    case ApplicationError::kRegistrationPreconditionFailed:
      return "An action failed due to the client not being registered.";
    case ApplicationError::kPolicyPersistenceImpossible:
      return "Policies can not be persisted to storage.";
    case ApplicationError::kPolicyPersistenceFailed:
      return "Failed to persist policies to storage.";
    case ApplicationError::kCannotAcquireLock:
      return "Failed to acquire global singleton lock.";
    case ApplicationError::kMojoConnectionFailed:
      return "A Mojo IPC connection could not be established.";
    case ApplicationError::kInstallationFailed:
      return "The application could not be installed/uninstalled.";
    case ApplicationError::kIpcCallerNotAllowed:
      return "The IPC caller is not allowed.";
    case ApplicationError::kCOMInitializationFailed:
      return "COM initialization failed.";
  }
}

std::string PosixErrnoToString(EnterpriseCompanionStatus::PosixErrno error) {
#if BUILDFLAG(IS_POSIX)
  return base::safe_strerror(error);
#else
  return base::StrCat({"Posix error code ", base::NumberToString(error), "."});
#endif
}

}  // namespace

PersistedError::PersistedError(int space,
                               int code,
                               const std::string& description)
    : space(space), code(code), description(description) {}
PersistedError::PersistedError(const PersistedError&) = default;
PersistedError::PersistedError(PersistedError&&) = default;
PersistedError::~PersistedError() = default;
PersistedError& PersistedError::operator=(const PersistedError&) = default;
PersistedError& PersistedError::operator=(PersistedError&&) = default;

std::string EnterpriseCompanionStatus::description() const {
  return std::visit(
      base::Overloaded{
          [](std::monostate) { return std::string("Success"); },
          [](const PersistedError& error) { return error.description; },
          [](policy::DeviceManagementStatus status) {
            return DeviceManagementStatusToString(status);
          },
          [](policy::CloudPolicyValidatorBase::Status status) {
            return std::string(
                policy::CloudPolicyValidatorBase::StatusToString(status));
          },
          [](ApplicationError error) {
            return ApplicationErrorToString(error);
          },
          [](PosixErrno error) { return PosixErrnoToString(error); },
      },
      status_variant_);
}

EnterpriseCompanionStatus::EnterpriseCompanionStatus(
    const EnterpriseCompanionStatus&) = default;
EnterpriseCompanionStatus::~EnterpriseCompanionStatus() = default;

EnterpriseCompanionStatus::EnterpriseCompanionStatus(ApplicationError error)
    : EnterpriseCompanionStatus(StatusVariant(error)) {}

EnterpriseCompanionStatus::EnterpriseCompanionStatus(
    StatusVariant status_variant)
    : status_variant_(std::move(status_variant)) {}

}  // namespace enterprise_companion
