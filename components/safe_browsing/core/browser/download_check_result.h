// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DOWNLOAD_CHECK_RESULT_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DOWNLOAD_CHECK_RESULT_H_

#include <string_view>

namespace safe_browsing {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// See SBClientDownloadCheckResult in
// //tools/metrics/histograms/metadata/sb_client/enums.xml
enum class DownloadCheckResult {
  UNKNOWN = 0,
  SAFE = 1,
  DANGEROUS = 2,
  UNCOMMON = 3,
  DANGEROUS_HOST = 4,
  POTENTIALLY_UNWANTED = 5,
  ALLOWLISTED_BY_POLICY = 6,
  ASYNC_SCANNING = 7,
  BLOCKED_PASSWORD_PROTECTED = 8,
  BLOCKED_TOO_LARGE = 9,
  SENSITIVE_CONTENT_WARNING = 10,
  SENSITIVE_CONTENT_BLOCK = 11,
  DEEP_SCANNED_SAFE = 12,
  PROMPT_FOR_SCANNING = 13,
  // Deprecated: BLOCKED_UNSUPPORTED_FILE_TYPE = 14,
  DANGEROUS_ACCOUNT_COMPROMISE = 15,
  DEEP_SCANNED_FAILED = 16,
  PROMPT_FOR_LOCAL_PASSWORD_SCANNING = 17,
  ASYNC_LOCAL_PASSWORD_SCANNING = 18,
  BLOCKED_SCAN_FAILED = 19,
  IMMEDIATE_DEEP_SCAN = 20,
  kMaxValue = IMMEDIATE_DEEP_SCAN,
};

std::string_view DownloadCheckResultToString(DownloadCheckResult result);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DOWNLOAD_CHECK_RESULT_H_
