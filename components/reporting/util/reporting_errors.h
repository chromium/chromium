// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_REPORTING_ERRORS_H_
#define COMPONENTS_REPORTING_UTIL_REPORTING_ERRORS_H_

namespace reporting {

inline constexpr char kUmaUnavailableErrorReason[] =
    "Browser.ERP.UnavailableErrorReason";

inline constexpr char kUmaDataLossErrorReason[] =
    "Browser.ERP.DataLossErrorReason";

// These enum values represent the different error messages associated with
// usages of `error::UNAVAILABLE` in Chrome. Anytime `error::UNAVAILABLE` is
// used, it should be UMA logged using this enum and
// kUmaUnavailableErrorReason.
//
// Update `UnavailableErrorReasonBrowser` in
// tools/metrics/histograms/metadata/browser/enums.xml when adding new values
// to this enum.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UnavailableErrorReason {
  CANNOT_GET_CLOUD_POLICY_MANAGER_FOR_BROWSER = 0,
  CANNOT_GET_CLOUD_POLICY_MANAGER_FOR_PROFILE = 1,
  CLIENT_NOT_CONNECTED_TO_MISSIVE = 2,
  DEVICE_DM_TOKEN_NOT_SET = 3,
  FAILED_TO_CREATE_STORAGE_QUEUE_DIRECTORY = 4,
  FILE_NOT_OPEN = 5,
  FILE_UPLOAD_DELEGATE_IS_NULL = 6,
  FILE_UPLOAD_JOB_DELEGATE_IS_NULL = 7,
  REPORTING_CLIENT_IS_NULL = 8,
  REPORT_QUEUE_DESTRUCTED = 9,
  REPORT_QUEUE_IS_NULL = 10,
  REPORT_QUEUE_PROVIDER_DESTRUCTED = 11,
  STORAGE_QUEUE_SHUTDOWN = 12,
  UNABLE_TO_BUILD_REPORT_QUEUE = 13,
  UPLOAD_PROVIDER_IS_NULL = 14,
  MAX_VALUE
};

// These enum values represent the different error messages associated with
// usages of `error::DATA_LOSS` in Chrome. Anytime `error::DATA_LOSS` is
// used, it should be UMA logged using this enum and
// kUmaDataLossErrorReason.
//
// Update DataLossErrorReason in
// tools/metrics/histograms/metadata/browser/enums.xml when adding new values.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DataLossErrorReason {
  REPORT_UPLOAD_FAILED = 0,
  REPORT_CLIENT_EMPTY_RESPONSE = 1,
  REPORT_CLIENT_BAD_RESPONSE_CODE = 2,
  NO_HEADERS_FOUND = 3,
  UNEXPECTED_UPLOAD_STATUS = 4,
  POST_REQUEST_FAILED = 5,
  NO_GRANULARITTY_RETURNED = 6,
  UNEXPECTED_GRANULARITY = 7,
  NO_UPLOAD_URL_RETURNED = 8,
  FAILED_TO_OPEN_UPLOAD_FILE = 9,
  CORRUPT_SESSION_TOKEN = 10,
  FILE_SIZE_MISMATCH = 11,
  CANNOT_READ_FILE = 12,
  NO_UPLOAD_SIZE_RETURNED = 13,
  UNEXPECTED_UPLOAD_RECEIVED_CODE = 14,
  NO_UPLOAD_ID_RETURNED = 15,
  UPLOAD_JOB_REMOVED = 16,
  JOB_LOST_SESSION_TOKEN = 17,
  JOB_BACKTRACKED = 18,
  JOB_INCOMPLETE = 19,
  CORRUPT_RESUMABLE_UPLOAD_URL = 20,
  FAILED_UPLOAD_CONTAINS_INVALID_SEQUENCE_INFORMATION = 21,
  SPECULATIVE_REPORT_QUEUE_DESTRUCTED_BEFORE_RECORDS_ENQUEUED = 22,
  FAILED_TO_CREATE_ENCRYPTION_KEY = 23,
  FAILED_TO_READ_HEALTH_DATA = 24,
  MISSING_GENERATION_ID = 25,
  FAILED_TO_PASE_GENERATION_ID = 26,
  INVALID_GENERATION_ID = 27,
  ALL_FILE_PATHS_MISSING_GENERATION_ID = 28,
  FAILED_TO_OPEN_STORAGE_QUEUE_FILE = 29,
  FAILED_TO_WRITE_METADATA = 30,
  FAILED_TO_READ_METADATA = 31,
  METADATA_GENERATION_ID_OUT_OF_RANGE = 32,
  METADATA_GENERATION_MISMATCH = 33,
  METADATA_LAST_RECORD_DIGEST_IS_CORRUPT = 34,
  FAILED_TO_RESTORE_LAST_RECORD_DIGEST = 35,
  FAILED_TO_SERIALIZE_WRAPPED_RECORD = 36,
  FAILED_TO_SERIALIZE_ENCRYPTED_RECORD = 37,
  FAILED_TO_OPEN_FILE = 38,
  FAILED_TO_GET_SIZE_OF_FILE = 39,
  FAILED_TO_READ_FILE = 40,
  FAILED_TO_WRITE_FILE = 41,
  FAILED_TO_OPEN_KEY_FILE = 42,
  FAILED_TO_SERIALIZE_KEY = 43,
  FAILED_TO_WRITE_KEY_FILE = 44,
  FAILED_TO_READ_FILE_INFO = 45,
  MAX_VALUE
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_REPORTING_ERRORS_H_
