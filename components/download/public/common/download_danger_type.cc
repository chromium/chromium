// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_danger_type.h"

#include "base/notreached.h"

namespace download {

// Converts DownloadDangerType into their corresponding string.
const char* GetDownloadDangerTypeString(const DownloadDangerType& danger_type) {
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return "DangerousFile";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return "DangerousURL";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return "DangerousContent";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return "DangerousHost";
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return "UncommonContent";
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return "PotentiallyUnwanted";
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      return "AsyncScanning";
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      return "BlockedPasswordProtected";
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return "BlockedTooLarge";
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return "SensitiveContentWarning";
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return "SensitiveContentBlock";
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return "DeepScannedFailed";
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
      return "DeepScannedSafe";
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return "DeepScannedOpenedDangerous";
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
      return "PromptForScanning";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return "DangerousAccountCompromise";
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
      return "NotDangerous";
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      return "MaybeDangerousContent";
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
      return "UserValidated";
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
      return "AllowlistedByPolicy";
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
      return "PromptForLocalPasswordScanning";
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
      return "AsyncLocalPasswordScanning";
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      return "BlockedScanFailed";
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace download
