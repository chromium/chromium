// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_DOWNLOAD_CONSTANTS_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_DOWNLOAD_CONSTANTS_H_

#include "components/history/core/browser/download_types.h"

namespace history {

// DownloadState represents the state of a DownloadRow saved into the
// DownloadDatabase. The values must not be changed as they are saved
// to disk in the database.
enum class DownloadState {
  INVALID = -1,
  IN_PROGRESS = 0,
  COMPLETE = 1,
  CANCELLED = 2,
  BUG_140687 = 3,
  INTERRUPTED = 4,
};

// DownloadDangerType represents the danger of a DownloadRow into the
// DownloadDatabase. The values must not be changed as they are saved
// to disk in the database.
enum class DownloadDangerType {
  INVALID = -1,
  NOT_DANGEROUS = 0,
  DANGEROUS_FILE = 1,
  DANGEROUS_URL = 2,
  DANGEROUS_CONTENT = 3,
  MAYBE_DANGEROUS_CONTENT = 4,
  UNCOMMON_CONTENT = 5,
  USER_VALIDATED = 6,
  DANGEROUS_HOST = 7,
  POTENTIALLY_UNWANTED = 8,
  WHITELISTED_BY_POLICY = 9,
  ASYNC_SCANNING = 10,
  BLOCKED_PASSWORD_PROTECTED = 11,
  BLOCKED_TOO_LARGE = 12,
  SENSITIVE_CONTENT_WARNING = 13,
  SENSITIVE_CONTENT_BLOCK = 14,
  DEEP_SCANNED_SAFE = 15,
  DEEP_SCANNED_OPENED_DANGEROUS = 16,
};

// DownloadId represents the id of a DownloadRow into the DownloadDatabase.
// The value is controlled by the embedder except for the reserved id
// kInvalidDownloadId.
extern const DownloadId kInvalidDownloadId;

}  // namespace

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_DOWNLOAD_CONSTANTS_H_
