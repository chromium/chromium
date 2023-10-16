// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_COMMON_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_COMMON_H_

#include "base/functional/callback.h"

namespace ash::carrier_lock {

// These values are stored in logs and reported in metrics. Entries must not be
// reordered or renumbered. New values should be added at the end.
// Corresponding name in enums.xml: CellularCarrierLockError.
enum class Result {
  kSuccess = 0,
  /* Modem setup errors */
  kInvalidSignature = 1,
  kInvalidImei = 2,
  kInvalidTimestamp = 3,
  kNetworkListTooLarge = 4,
  kAlgorithmNotSupported = 5,
  kFeatureNotSupported = 6,
  kDecodeOrParsingError = 7,
  kHandlerNotInitialized = 8,
  kOperationNotSupported = 9,
  kModemInternalError = 10,
  /* Manager initialization errors */
  kInvalidNetworkHandler = 11,
  kInvalidModemHandler = 12,
  kInvalidAuxHandlers = 13,
  kModemNotFound = 14,
  kSerialProviderFailed = 15,
  /* Common errors for FCM/PSM/Config handlers */
  kHandlerBusy = 16,
  kRequestFailed = 17,
  kInitializationFailed = 18,
  kConnectionError = 19,
  kInvalidInput = 20,
  kServerInternalError = 21,
  kInvalidResponse = 22,
  /* PSM specific errors */
  kCreatePsmClientFailed = 23,
  kCreateOprfRequestFailed = 24,
  kInvalidOprfReply = 25,
  kCreateQueryRequestFailed = 26,
  kInvalidQueryReply = 27,
  /* Config parsing errors */
  kNoLockConfiguration = 28,
  kInvalidConfiguration = 29,
  kLockedWithoutTopic = 30,
  kEmptySignedConfiguration = 31,

  kMaxValue = kEmptySignedConfiguration
};

using Callback = base::OnceCallback<void(Result result)>;

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_COMMON_H_
