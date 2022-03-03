// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HERMES_HERMES_RESPONSE_STATUS_H_
#define CHROMEOS_DBUS_HERMES_HERMES_RESPONSE_STATUS_H_

#include <string>

#include "base/callback.h"

namespace chromeos {

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
  kMaxValue = kErrorNoResponse
};

// Callback that receives only a HermesResponseStatus.
using HermesResponseCallback =
    base::OnceCallback<void(HermesResponseStatus status)>;

// Returns the HermesResponseStatus corresponding to the
// given dbus_|error_name|.
HermesResponseStatus HermesResponseStatusFromErrorName(
    const std::string& error_name);

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash {
using ::chromeos::HermesResponseStatus;
}

#endif  // CHROMEOS_DBUS_HERMES_HERMES_RESPONSE_STATUS_H_
