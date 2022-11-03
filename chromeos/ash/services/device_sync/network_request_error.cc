// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/network_request_error.h"

namespace ash {

namespace device_sync {

std::ostream& operator<<(std::ostream& stream,
                         const NetworkRequestError& error) {
  switch (error) {
    case NetworkRequestError::kOffline:
      stream << "[offline]";
      break;
    case NetworkRequestError::kEndpointNotFound:
      stream << "[endpoint not found]";
      break;
    case NetworkRequestError::kAuthenticationError:
      stream << "[authentication error]";
      break;
    case NetworkRequestError::kBadRequest:
      stream << "[bad request]";
      break;
    case NetworkRequestError::kResponseMalformed:
      stream << "[response malformed]";
      break;
    case NetworkRequestError::kInternalServerError:
      stream << "[internal server error]";
      break;
    case NetworkRequestError::kUnknown:
      stream << "[unknown]";
      break;
  }
  return stream;
}

}  // namespace device_sync

}  // namespace ash
