// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hermes/hermes_response_status.h"

#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace chromeos {

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
  if (error_name == hermes::kErrorNoResponse) {
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

  return HermesResponseStatus::kErrorUnknown;
}

}  // namespace chromeos
