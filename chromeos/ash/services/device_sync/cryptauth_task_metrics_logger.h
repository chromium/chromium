// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_TASK_METRICS_LOGGER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_TASK_METRICS_LOGGER_H_

#include <string>

#include "chromeos/ash/services/device_sync/network_request_error.h"

namespace ash {

namespace device_sync {

// A group of functions and enums used to log success metrics for individual
// tasks in the CryptAuth v2 Enrollment and v2 DeviceSync flows.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class CryptAuthAsyncTaskResult {
  kSuccess = 0,
  kTimeout = 1,
  kError = 2,
  kMaxValue = kError
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class CryptAuthApiCallResult {
  kSuccess = 0,
  kTimeout = 1,
  kNetworkRequestErrorOffline = 2,
  kNetworkRequestErrorEndpointNotFound = 3,
  kNetworkRequestErrorAuthenticationError = 4,
  kNetworkRequestErrorBadRequest = 5,
  kNetworkRequestErrorResponseMalformed = 6,
  kNetworkRequestErrorInternalServerError = 7,
  kNetworkRequestErrorUnknown = 8,
  kMaxValue = kNetworkRequestErrorUnknown
};

CryptAuthApiCallResult CryptAuthApiCallResultFromNetworkRequestError(
    NetworkRequestError network_request_error);

void LogCryptAuthAsyncTaskSuccessMetric(const std::string& metric_name,
                                        CryptAuthAsyncTaskResult result);

void LogCryptAuthApiCallSuccessMetric(const std::string& metric_name,
                                      CryptAuthApiCallResult result);

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_TASK_METRICS_LOGGER_H_
