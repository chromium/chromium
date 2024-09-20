// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/download_check_result.h"

namespace safe_browsing {

std::string_view DownloadCheckResultToString(DownloadCheckResult result) {
  switch (result) {
    case DownloadCheckResult::UNKNOWN:
      return "UNKNOWN";
    case DownloadCheckResult::SAFE:
      return "SAFE";
    case DownloadCheckResult::DANGEROUS:
      return "DANGEROUS";
    case DownloadCheckResult::UNCOMMON:
      return "UNCOMMON";
    case DownloadCheckResult::DANGEROUS_HOST:
      return "DANGEROUS_HOST";
    case DownloadCheckResult::POTENTIALLY_UNWANTED:
      return "POTENTIALLY_UNWANTED";
    case DownloadCheckResult::ALLOWLISTED_BY_POLICY:
      return "ALLOWLISTED_BY_POLICY";
    case DownloadCheckResult::ASYNC_SCANNING:
      return "ASYNC_SCANNING";
    case DownloadCheckResult::ASYNC_LOCAL_PASSWORD_SCANNING:
      return "ASYNC_LOCAL_PASSWORD_SCANNING";
    case DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED:
      return "BLOCKED_PASSWORD_PROTECTED";
    case DownloadCheckResult::BLOCKED_TOO_LARGE:
      return "BLOCKED_TOO_LARGE";
    case DownloadCheckResult::SENSITIVE_CONTENT_WARNING:
      return "SENSITIVE_CONTENT_WARNING";
    case DownloadCheckResult::SENSITIVE_CONTENT_BLOCK:
      return "SENSITIVE_CONTENT_BLOCK";
    case DownloadCheckResult::DEEP_SCANNED_SAFE:
      return "DEEP_SCANNED_SAFE";
    case DownloadCheckResult::PROMPT_FOR_SCANNING:
      return "PROMPT_FOR_SCANNING";
    case DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE:
      return "DANGEROUS_ACCOUNT_COMPROMISE";
    case DownloadCheckResult::DEEP_SCANNED_FAILED:
      return "DEEP_SCANNED_FAILED";
    case DownloadCheckResult::PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
      return "PROMPT_FOR_LOCAL_PASSWORD_SCANNING";
    case DownloadCheckResult::BLOCKED_SCAN_FAILED:
      return "BLOCKED_SCAN_FAILED";
    case DownloadCheckResult::IMMEDIATE_DEEP_SCAN:
      return "IMMEDIATE_DEEP_SCAN";
  }
}

}  // namespace safe_browsing
