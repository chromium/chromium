// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CLIENT_NEARBY_HTTP_RESULT_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CLIENT_NEARBY_HTTP_RESULT_H_

#include <optional>
#include <ostream>
#include <string>

#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace ash::nearby {

enum class NearbyHttpError {
  // Request could not be completed because the device is offline or has issues
  // sending the HTTP request.
  kOffline,

  // Server endpoint could not be found.
  kEndpointNotFound,

  // Authentication error contacting back-end.
  kAuthenticationError,

  // Request was invalid.
  kBadRequest,

  // The server responded, but the response was not formatted correctly.
  kResponseMalformed,

  // Internal server error.
  kInternalServerError,

  // Unknown result.
  kUnknown
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync with
// the NearbyHttpResult enum in
// //tools/metrics/histograms/metadata/nearby/enums.xml.
enum class NearbyHttpResult {
  kSuccess = 0,
  kTimeout = 1,
  kHttpErrorOffline = 2,
  kHttpErrorEndpointNotFound = 3,
  kHttpErrorAuthenticationError = 4,
  kHttpErrorBadRequest = 5,
  kHttpErrorResponseMalformed = 6,
  kHttpErrorInternalServerError = 7,
  kHttpErrorUnknown = 8,
  kMaxValue = kHttpErrorUnknown
};

class NearbyHttpStatus {
 public:
  NearbyHttpStatus(const int net_error,
                   const network::mojom::URLResponseHead* head);
  NearbyHttpStatus(const NearbyHttpStatus& status);
  ~NearbyHttpStatus();

  bool IsSuccess() const;
  int GetResultCodeForMetrics() const;
  std::string ToString() const;

 private:
  enum class Status { kSuccess, kNetworkFailure, kHttpFailure } status_;
  int net_error_code_;
  std::optional<int> http_response_code_;
};

NearbyHttpError NearbyHttpErrorForHttpResponseCode(int response_code);
NearbyHttpResult NearbyHttpErrorToResult(NearbyHttpError error);

std::ostream& operator<<(std::ostream& stream, const NearbyHttpResult& result);
std::ostream& operator<<(std::ostream& stream, const NearbyHttpError& error);
std::ostream& operator<<(std::ostream& stream, const NearbyHttpStatus& status);

}  // namespace ash::nearby

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CLIENT_NEARBY_HTTP_RESULT_H_
