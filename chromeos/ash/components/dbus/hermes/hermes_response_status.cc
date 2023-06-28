// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "dbus/message.h"

#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash {

const std::array<HermesResponseStatus, 4> kHermesUserErrorCodes = {
    HermesResponseStatus::kErrorMalformedResponse,
    HermesResponseStatus::kErrorAlreadyDisabled,
    HermesResponseStatus::kErrorAlreadyEnabled,
    HermesResponseStatus::kErrorInvalidActivationCode};

HermesResponseStatus HermesResponseStatusFromErrorName(
    const std::string& error_name) {
  if (error_name == hermes::kErrorAlreadyDisabled) {
    return HermesResponseStatus::kErrorAlreadyDisabled;
  }
  if (error_name == hermes::kErrorAlreadyEnabled) {
    return HermesResponseStatus::kErrorAlreadyEnabled;
  }
  if (error_name == hermes::kErrorInvalidActivationCode) {
    return HermesResponseStatus::kErrorInvalidActivationCode;
  }
  if (error_name == hermes::kErrorInvalidIccid) {
    return HermesResponseStatus::kErrorInvalidIccid;
  }
  if (error_name == hermes::kErrorInvalidParameter) {
    return HermesResponseStatus::kErrorInvalidParameter;
  }
  if (error_name == hermes::kErrorNeedConfirmationCode) {
    return HermesResponseStatus::kErrorNeedConfirmationCode;
  }
  if (error_name == hermes::kErrorSendNotificationFailure) {
    return HermesResponseStatus::kErrorSendNotificationFailure;
  }
  if (error_name == hermes::kErrorTestProfileInProd) {
    return HermesResponseStatus::kErrorTestProfileInProd;
  }
  if (error_name == hermes::kErrorUnsupported) {
    return HermesResponseStatus::kErrorUnsupported;
  }
  if (error_name == hermes::kErrorWrongState) {
    return HermesResponseStatus::kErrorWrongState;
  }
  if (error_name == hermes::kErrorNoResponse ||
      error_name == DBUS_ERROR_NO_REPLY) {
    return HermesResponseStatus::kErrorNoResponse;
  }
  if (error_name == hermes::kErrorMalformedResponse) {
    return HermesResponseStatus::kErrorMalformedResponse;
  }
  if (error_name == hermes::kErrorInternalLpaFailure) {
    return HermesResponseStatus::kErrorInternalLpaFailure;
  }
  if (error_name == hermes::kErrorBadRequest) {
    return HermesResponseStatus::kErrorBadRequest;
  }
  if (error_name == hermes::kErrorBadNotification) {
    return HermesResponseStatus::kErrorBadNotification;
  }
  if (error_name == hermes::kErrorPendingProfile) {
    return HermesResponseStatus::kErrorPendingProfile;
  }
  if (error_name == hermes::kErrorSendApduFailure) {
    return HermesResponseStatus::kErrorSendApduFailure;
  }
  if (error_name == hermes::kErrorSendHttpsFailure) {
    return HermesResponseStatus::kErrorSendHttpsFailure;
  }
  if (error_name == hermes::kErrorUnexpectedModemManagerState) {
    return HermesResponseStatus::kErrorUnexpectedModemManagerState;
  }
  if (error_name == hermes::kErrorModemMessageProcessing) {
    return HermesResponseStatus::kErrorModemMessageProcessing;
  }
  if (error_name == hermes::kErrorUnknown) {
    return HermesResponseStatus::kErrorUnknown;
  }
  if (error_name.empty()) {
    return HermesResponseStatus::kErrorEmptyResponse;
  }

  return HermesResponseStatus::kErrorUnknownResponse;
}

std::ostream& operator<<(std::ostream& stream, HermesResponseStatus status) {
  switch (status) {
    case HermesResponseStatus::kSuccess:
      return stream << "[kSuccess]";
    case HermesResponseStatus::kErrorAlreadyDisabled:
      return stream << "[kErrorAlreadyDisabled]";
    case HermesResponseStatus::kErrorAlreadyEnabled:
      return stream << "[kErrorAlreadyEnabled]";
    case HermesResponseStatus::kErrorInvalidActivationCode:
      return stream << "[kErrorInvalidActivationCode]";
    case HermesResponseStatus::kErrorInvalidIccid:
      return stream << "[kErrorInvalidIccid]";
    case HermesResponseStatus::kErrorInvalidParameter:
      return stream << "[kErrorInvalidParameter]";
    case HermesResponseStatus::kErrorNeedConfirmationCode:
      return stream << "[kErrorNeedConfirmationCode]";
    case HermesResponseStatus::kErrorSendNotificationFailure:
      return stream << "[kErrorSendNotificationFailure]";
    case HermesResponseStatus::kErrorTestProfileInProd:
      return stream << "[kErrorTestProfileInProd]";
    case HermesResponseStatus::kErrorUnknown:
      return stream << "[kErrorUnknown]";
    case HermesResponseStatus::kErrorUnsupported:
      return stream << "[kErrorUnsupported]";
    case HermesResponseStatus::kErrorWrongState:
      return stream << "[kErrorWrongState]";
    case HermesResponseStatus::kErrorInvalidResponse:
      return stream << "[kErrorInvalidResponse]";
    case HermesResponseStatus::kErrorNoResponse:
      return stream << "[kErrorNoResponse]";
    case HermesResponseStatus::kErrorMalformedResponse:
      return stream << "[kErrorMalformedResponse]";
    case HermesResponseStatus::kErrorInternalLpaFailure:
      return stream << "[kErrorInternalLpaFailure]";
    case HermesResponseStatus::kErrorBadRequest:
      return stream << "[kErrorBadRequest]";
    case HermesResponseStatus::kErrorBadNotification:
      return stream << "[kErrorBadNotification]";
    case HermesResponseStatus::kErrorPendingProfile:
      return stream << "[kErrorPendingProfile]";
    case HermesResponseStatus::kErrorSendApduFailure:
      return stream << "[kErrorSendApduFailure]";
    case HermesResponseStatus::kErrorSendHttpsFailure:
      return stream << "[kErrorSendHttpsFailure]";
    case HermesResponseStatus::kErrorUnexpectedModemManagerState:
      return stream << "[kErrorUnexpectedModemManagerState]";
    case HermesResponseStatus::kErrorModemMessageProcessing:
      return stream << "[kErrorModemMessageProcessing]";
    case HermesResponseStatus::kErrorEmptyResponse:
      return stream << "[kErrorEmptyResponse]";
    case HermesResponseStatus::kErrorUnknownResponse:
      return stream << "[kErrorUnknownResponse]";
  }
  return stream << (static_cast<int>(status));
}

}  // namespace ash
