// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_RESPONSE_STATUS_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_RESPONSE_STATUS_H_

#include <array>
#include <ostream>
#include <string>
#include "base/component_export.h"
#include "base/functional/callback.h"

namespace ash {

// Enum values the represent response status of hermes client method calls.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HermesResponseStatus {
  kSuccess = 0,
  kErrorAlreadyDisabled = 1,
  kErrorAlreadyEnabled = 2,
  kErrorInvalidActivationCode = 3,
  kErrorInvalidIccid = 4,
  kErrorInvalidParameter = 5,
  kErrorNeedConfirmationCode = 6,
  kErrorSendNotificationFailure = 7,
  kErrorTestProfileInProd = 8,
  kErrorUnknown = 9,
  kErrorUnsupported = 10,
  kErrorWrongState = 11,
  kErrorInvalidResponse = 12,
  kErrorNoResponse = 13,
  kErrorMalformedResponse = 14,
  kErrorInternalLpaFailure = 15,
  kErrorBadRequest = 16,
  kErrorBadNotification = 17,
  kErrorPendingProfile = 18,
  kErrorSendApduFailure = 19,
  kErrorSendHttpsFailure = 20,
  kErrorUnexpectedModemManagerState = 21,
  kErrorModemMessageProcessing = 22,
  kErrorEmptyResponse = 23,
  kErrorUnknownResponse = 24,
  kMaxValue = kErrorUnknownResponse
};

// Hermes codes returned that are possibly a result of user error.
extern const std::array<HermesResponseStatus, 4> COMPONENT_EXPORT(HERMES_CLIENT)
    kHermesUserErrorCodes;

// Callback that receives only a HermesResponseStatus.
using HermesResponseCallback =
    base::OnceCallback<void(HermesResponseStatus status)>;

// Returns the HermesResponseStatus corresponding to the
// given dbus_|error_name|.
HermesResponseStatus COMPONENT_EXPORT(HERMES_CLIENT)
    HermesResponseStatusFromErrorName(const std::string& error_name);

std::ostream& COMPONENT_EXPORT(HERMES_CLIENT) operator<<(
    std::ostream& stream,
    HermesResponseStatus status);
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_RESPONSE_STATUS_H_
