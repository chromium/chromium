// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_DANGER_TYPE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_DANGER_TYPE_H_

namespace download {

// This enum is also used by histograms.  Do not change the ordering or remove
// items.
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

  // Download URL whitelisted by enterprise policy.
  DOWNLOAD_DANGER_TYPE_WHITELISTED_BY_POLICY = 9,

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

  // Memory space for histograms is determined by the max.
  // ALWAYS ADD NEW VALUES BEFORE THIS ONE.
  DOWNLOAD_DANGER_TYPE_MAX
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_DANGER_TYPE_H_
