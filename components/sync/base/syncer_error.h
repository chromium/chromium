// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNCER_ERROR_H_
#define COMPONENTS_SYNC_BASE_SYNCER_ERROR_H_

#include <string>

namespace syncer {

// This class describes all the possible results of a sync cycle. It should be
// passed by value.
class SyncerError {
 public:
  // This enum should be in sync with SyncerErrorValues in enums.xml. These
  // values are persisted to logs. Entries should not be renumbered and numeric
  // values should never be reused.
  enum Value {
    UNSET = 0,  // Default value.
    // Deprecated: CANNOT_DO_WORK = 1,

    NETWORK_CONNECTION_UNAVAILABLE = 2,  // Connectivity failure.
    NETWORK_IO_ERROR = 3,                // Response buffer read error.
    SYNC_SERVER_ERROR = 4,               // Non auth HTTP error.
    SYNC_AUTH_ERROR = 5,                 // HTTP auth error.

    // Based on values returned by server.  Most are defined in sync.proto.
    // Deprecated: SERVER_RETURN_INVALID_CREDENTIAL = 6,
    SERVER_RETURN_UNKNOWN_ERROR = 7,
    SERVER_RETURN_THROTTLED = 8,
    SERVER_RETURN_TRANSIENT_ERROR = 9,
    SERVER_RETURN_MIGRATION_DONE = 10,
    SERVER_RETURN_CLEAR_PENDING = 11,
    SERVER_RETURN_NOT_MY_BIRTHDAY = 12,
    SERVER_RETURN_CONFLICT = 13,
    SERVER_RESPONSE_VALIDATION_FAILED = 14,
    SERVER_RETURN_DISABLED_BY_ADMIN = 15,
    // Deprecated: SERVER_RETURN_USER_ROLLBACK = 16,
    // Deprecated: SERVER_RETURN_PARTIAL_FAILURE = 17,
    SERVER_RETURN_CLIENT_DATA_OBSOLETE = 18,
    SERVER_RETURN_ENCRYPTION_OBSOLETE = 19,

    // Deprecated: DATATYPE_TRIGGERED_RETRY = 20,

    SERVER_MORE_TO_DOWNLOAD = 21,

    SYNCER_OK = 22,

    kMaxValue = SYNCER_OK,
  };

  constexpr SyncerError() = default;
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
  // TODO(crbug.com/947443): Consider storing the actual enums.
  int net_error_code_ = 0;
  int http_status_code_ = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SYNCER_ERROR_H_
