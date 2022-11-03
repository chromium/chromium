// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_task_metrics_logger.h"

#include "base/metrics/histogram_functions.h"

namespace ash {

namespace device_sync {

CryptAuthApiCallResult CryptAuthApiCallResultFromNetworkRequestError(
    NetworkRequestError network_request_error) {
  switch (network_request_error) {
    case NetworkRequestError::kOffline:
      return CryptAuthApiCallResult::kNetworkRequestErrorOffline;
    case NetworkRequestError::kEndpointNotFound:
      return CryptAuthApiCallResult::kNetworkRequestErrorEndpointNotFound;
    case NetworkRequestError::kAuthenticationError:
      return CryptAuthApiCallResult::kNetworkRequestErrorAuthenticationError;
    case NetworkRequestError::kBadRequest:
      return CryptAuthApiCallResult::kNetworkRequestErrorBadRequest;
    case NetworkRequestError::kResponseMalformed:
      return CryptAuthApiCallResult::kNetworkRequestErrorResponseMalformed;
    case NetworkRequestError::kInternalServerError:
      return CryptAuthApiCallResult::kNetworkRequestErrorInternalServerError;
    case NetworkRequestError::kUnknown:
      return CryptAuthApiCallResult::kNetworkRequestErrorUnknown;
  }
}

void LogCryptAuthAsyncTaskSuccessMetric(const std::string& metric_name,
                                        CryptAuthAsyncTaskResult result) {
  base::UmaHistogramEnumeration(metric_name, result);
}

void LogCryptAuthApiCallSuccessMetric(const std::string& metric_name,
                                      CryptAuthApiCallResult result) {
  base::UmaHistogramEnumeration(metric_name, result);
}

}  // namespace device_sync

}  // namespace ash
