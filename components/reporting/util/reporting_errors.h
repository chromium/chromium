// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_REPORTING_ERRORS_H_
#define COMPONENTS_REPORTING_UTIL_REPORTING_ERRORS_H_

namespace reporting {

inline constexpr char kUmaUnavailableErrorReason[] =
    "Browser.ERP.UnavailableErrorReason";

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
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_REPORTING_ERRORS_H_
