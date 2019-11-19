// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNCER_ERROR_H_
#define COMPONENTS_SYNC_BASE_SYNCER_ERROR_H_

#include <string>

#include "base/logging.h"

namespace syncer {

// This class describes all the possible results of a sync cycle. It should be
// passed by value.
class SyncerError {
 public:
  enum Value {
    UNSET = 0,       // Default value.
    CANNOT_DO_WORK,  // A model worker could not process a work item.

    NETWORK_CONNECTION_UNAVAILABLE,  // Connectivity failure.
    NETWORK_IO_ERROR,                // Response buffer read error.
    SYNC_SERVER_ERROR,               // Non auth HTTP error.
    SYNC_AUTH_ERROR,                 // HTTP auth error.

    // Based on values returned by server.  Most are defined in sync.proto.
    // TODO(crbug.com/951350): Unused, remove.
    SERVER_RETURN_INVALID_CREDENTIAL,
    SERVER_RETURN_UNKNOWN_ERROR,
    SERVER_RETURN_THROTTLED,
    SERVER_RETURN_TRANSIENT_ERROR,
    SERVER_RETURN_MIGRATION_DONE,
    SERVER_RETURN_CLEAR_PENDING,
    SERVER_RETURN_NOT_MY_BIRTHDAY,
    SERVER_RETURN_CONFLICT,
    SERVER_RESPONSE_VALIDATION_FAILED,
    SERVER_RETURN_DISABLED_BY_ADMIN,
    // TODO(crbug.com/951350): Unused, remove.
    SERVER_RETURN_USER_ROLLBACK,
    SERVER_RETURN_PARTIAL_FAILURE,
    SERVER_RETURN_CLIENT_DATA_OBSOLETE,

    // A datatype decided the sync cycle needed to be performed again.
    DATATYPE_TRIGGERED_RETRY,

    SERVER_MORE_TO_DOWNLOAD,

    SYNCER_OK
  };

  constexpr SyncerError() {}
  // Note: NETWORK_CONNECTION_UNAVAILABLE, SYNC_SERVER_ERROR, and
  // SYNC_AUTH_ERROR are *not* valid inputs for this constructor. These types
  // of errors must be created via the factory functions below.
  explicit SyncerError(Value value);

  static SyncerError NetworkConnectionUnavailable(int net_error_code);
  static SyncerError HttpError(int http_status_code);

  Value value() const { return value_; }

  std::string ToString() const;

  // Helper to check that |error| is set to something (not UNSET) and is not
  // SYNCER_OK or SERVER_MORE_TO_DOWNLOAD.
  bool IsActualError() const;

 private:
  constexpr SyncerError(Value value, int net_error_code, int http_status_code)
      : value_(value),
        net_error_code_(net_error_code),
        http_status_code_(http_status_code) {}

  Value value_ = UNSET;
  int net_error_code_ = 0;
  int http_status_code_ = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SYNCER_ERROR_H_
