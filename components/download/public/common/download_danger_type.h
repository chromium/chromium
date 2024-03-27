// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_DANGER_TYPE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_DANGER_TYPE_H_

#include "components/download/public/common/download_export.h"

namespace download {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Update enum names in
// tools/metrics/histograms/metadata/download/enums.xml, and variants
// in tools/metrics/histograms/metadata/download/histograms.xml on additions.
enum DownloadDangerType {
  // The download is safe.
  DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS = 0,

  // A dangerous file to the system (e.g.: a pdf or extension from
  // places other than gallery).
  DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE = 1,

  // Safebrowsing download service shows this URL leads to malicious file
  // download.
  DOWNLOAD_DANGER_TYPE_DANGEROUS_URL = 2,

  // SafeBrowsing download service shows this file content as being malicious.
  DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT = 3,

  // The content of this download may be malicious (e.g., extension is exe but
  // SafeBrowsing has not finished checking the content).
  DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT = 4,

  // SafeBrowsing download service checked the contents of the download, but
  // didn't have enough data to determine whether it was malicious.
  DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT = 5,

  // The download was evaluated to be one of the other types of danger,
  // but the user told us to go ahead anyway.
  DOWNLOAD_DANGER_TYPE_USER_VALIDATED = 6,

  // SafeBrowsing download service checked the contents of the download and
  // didn't have data on this specific file, but the file was served from a host
  // known to serve mostly malicious content.
  DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST = 7,

  // Applications and extensions that modify browser and/or computer settings
  DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED = 8,

  // Download URL allowed by enterprise policy.
  DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY = 9,

  // Download is pending a more detailed verdict.
  DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING = 10,

  // Download is password protected, and should be blocked according to policy.
  DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED = 11,

  // Download is too large, and should be blocked according to policy. See the
  // BlockLargeFileTransfer policy for details.
  DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE = 12,

  // Download deep scanning identified sensitive content, and recommended
  // warning the user.
  DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING = 13,

  // Download deep scanning identified sensitive content, and recommended
  // blocking the file.
  DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK = 14,

  // Download deep scanning identified no problems.
  DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE = 15,

  // Download deep scanning identified a problem, but the file has already been
  // opened by the user.
  DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS = 16,

  // The user is enrolled in Enhanced Safe Browsing or the Advanced Protection
  // Program, and the server has recommended this file be deep scanned.
  DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING = 17,

  // Deprecated: DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE = 18,

  // SafeBrowsing download service has classified this file as being associated
  // with account compromise through stealing cookies.
  DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE = 19,

  // The user has chosen to deep scan this file, but the scan has failed. The
  // safety of this download is unknown.
  DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED = 20,

  // The server has recommend this encrypted archive prompt the user for a
  // pssword to use locally for further scanning.
  DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING = 21,

  // Download is pending a more detailed verdict after a prompt to use the
  // password locally for further scanning.
  DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING = 22,

  // Download scan is unsuccessful, and should be blocked according to the
  // policy.
  DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED = 23,

  // Memory space for histograms is determined by the max.
  // ALWAYS ADD NEW VALUES BEFORE THIS ONE.
  DOWNLOAD_DANGER_TYPE_MAX
};

// Converts DownloadDangerType into their corresponding string, used only
// for metrics.
COMPONENTS_DOWNLOAD_EXPORT
const char* GetDownloadDangerTypeString(const DownloadDangerType& danger_type);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_DANGER_TYPE_H_
