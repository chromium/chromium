// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/content/browser/download_conversions.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/history/core/browser/download_constants.h"

namespace history {

download::DownloadItem::DownloadState ToContentDownloadState(
    DownloadState state) {
  switch (state) {
    case DownloadState::IN_PROGRESS:
      return download::DownloadItem::IN_PROGRESS;
    case DownloadState::COMPLETE:
      return download::DownloadItem::COMPLETE;
    case DownloadState::CANCELLED:
      return download::DownloadItem::CANCELLED;
    case DownloadState::INTERRUPTED:
      return download::DownloadItem::INTERRUPTED;
    case DownloadState::INVALID:
    case DownloadState::BUG_140687:
      NOTREACHED_IN_MIGRATION();
      return download::DownloadItem::MAX_DOWNLOAD_STATE;
  }
  NOTREACHED_IN_MIGRATION();
  return download::DownloadItem::MAX_DOWNLOAD_STATE;
}

DownloadState ToHistoryDownloadState(
    download::DownloadItem::DownloadState state) {
  switch (state) {
    case download::DownloadItem::IN_PROGRESS:
      return DownloadState::IN_PROGRESS;
    case download::DownloadItem::COMPLETE:
      return DownloadState::COMPLETE;
    case download::DownloadItem::CANCELLED:
      return DownloadState::CANCELLED;
    case download::DownloadItem::INTERRUPTED:
      return DownloadState::INTERRUPTED;
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
      return DownloadState::INVALID;
  }
  NOTREACHED_IN_MIGRATION();
  return DownloadState::INVALID;
}

download::DownloadDangerType ToContentDownloadDangerType(
    DownloadDangerType danger_type) {
  switch (danger_type) {
    case DownloadDangerType::NOT_DANGEROUS:
      return download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
    case DownloadDangerType::DANGEROUS_FILE:
      return download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
    case DownloadDangerType::DANGEROUS_URL:
      return download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL;
    case DownloadDangerType::DANGEROUS_CONTENT:
      return download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT;
    case DownloadDangerType::MAYBE_DANGEROUS_CONTENT:
      return download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
    case DownloadDangerType::UNCOMMON_CONTENT:
      return download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT;
    case DownloadDangerType::USER_VALIDATED:
      return download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED;
    case DownloadDangerType::DANGEROUS_HOST:
      return download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST;
    case DownloadDangerType::POTENTIALLY_UNWANTED:
      return download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED;
    case DownloadDangerType::ALLOWLISTED_BY_POLICY:
      return download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY;
    case DownloadDangerType::ASYNC_SCANNING:
      return download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING;
    case DownloadDangerType::ASYNC_LOCAL_PASSWORD_SCANNING:
      return download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING;
    case DownloadDangerType::BLOCKED_PASSWORD_PROTECTED:
      return download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED;
    case DownloadDangerType::BLOCKED_TOO_LARGE:
      return download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE;
    case DownloadDangerType::SENSITIVE_CONTENT_WARNING:
      return download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING;
    case DownloadDangerType::SENSITIVE_CONTENT_BLOCK:
      return download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK;
    case DownloadDangerType::DEEP_SCANNED_SAFE:
      return download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE;
    case DownloadDangerType::DEEP_SCANNED_OPENED_DANGEROUS:
      return download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS;
    case DownloadDangerType::PROMPT_FOR_SCANNING:
      return download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING;
    case DownloadDangerType::BLOCKED_UNSUPPORTED_FILETYPE:
      // BLOCKED_UNSUPPORTED_FILETYPE has been deprecated in
      // https://crbug.com/330373911. Any remaining entries in history are
      // mapped to NOT_DANGEROUS. Since these downloads were canceled at
      // shutdown, this does not result in any user-visible change.
      return download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
    case DownloadDangerType::DANGEROUS_ACCOUNT_COMPROMISE:
      return download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE;
    case DownloadDangerType::DEEP_SCANNED_FAILED:
      return download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED;
    case DownloadDangerType::PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
      return download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING;
    case DownloadDangerType::BLOCKED_SCAN_FAILED:
      return download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED;
    case DownloadDangerType::INVALID:
      NOTREACHED_IN_MIGRATION();
      return download::DOWNLOAD_DANGER_TYPE_MAX;
  }
}

DownloadDangerType ToHistoryDownloadDangerType(
    download::DownloadDangerType danger_type) {
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
      return DownloadDangerType::NOT_DANGEROUS;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return DownloadDangerType::DANGEROUS_FILE;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return DownloadDangerType::DANGEROUS_URL;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return DownloadDangerType::DANGEROUS_CONTENT;
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      return DownloadDangerType::MAYBE_DANGEROUS_CONTENT;
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return DownloadDangerType::UNCOMMON_CONTENT;
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
      return DownloadDangerType::USER_VALIDATED;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return DownloadDangerType::DANGEROUS_HOST;
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return DownloadDangerType::POTENTIALLY_UNWANTED;
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
      return DownloadDangerType::ALLOWLISTED_BY_POLICY;
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      return DownloadDangerType::ASYNC_SCANNING;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      return DownloadDangerType::BLOCKED_PASSWORD_PROTECTED;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return DownloadDangerType::BLOCKED_TOO_LARGE;
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return DownloadDangerType::SENSITIVE_CONTENT_WARNING;
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return DownloadDangerType::SENSITIVE_CONTENT_BLOCK;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
      return DownloadDangerType::DEEP_SCANNED_SAFE;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return DownloadDangerType::DEEP_SCANNED_OPENED_DANGEROUS;
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
      return DownloadDangerType::PROMPT_FOR_SCANNING;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return DownloadDangerType::DANGEROUS_ACCOUNT_COMPROMISE;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return DownloadDangerType::DEEP_SCANNED_FAILED;
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
      return DownloadDangerType::PROMPT_FOR_LOCAL_PASSWORD_SCANNING;
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
      return DownloadDangerType::ASYNC_LOCAL_PASSWORD_SCANNING;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      return DownloadDangerType::BLOCKED_SCAN_FAILED;
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED_IN_MIGRATION();
      return DownloadDangerType::INVALID;
  }
}

download::DownloadInterruptReason ToContentDownloadInterruptReason(
    DownloadInterruptReason interrupt_reason) {
  return static_cast<download::DownloadInterruptReason>(interrupt_reason);
}

DownloadInterruptReason ToHistoryDownloadInterruptReason(
    download::DownloadInterruptReason interrupt_reason) {
  return static_cast<DownloadInterruptReason>(interrupt_reason);
}

uint32_t ToContentDownloadId(DownloadId id) {
  DCHECK_NE(id, kInvalidDownloadId);
  return static_cast<uint32_t>(id);
}

DownloadId ToHistoryDownloadId(uint32_t id) {
  DCHECK_NE(id, download::DownloadItem::kInvalidId);
  return static_cast<DownloadId>(id);
}

std::vector<download::DownloadItem::ReceivedSlice> ToContentReceivedSlices(
    const std::vector<DownloadSliceInfo>& slice_infos) {
  std::vector<download::DownloadItem::ReceivedSlice> result;

  for (const auto& slice_info : slice_infos) {
    result.emplace_back(slice_info.offset, slice_info.received_bytes,
                        slice_info.finished);
  }

  return result;
}

std::vector<DownloadSliceInfo> GetHistoryDownloadSliceInfos(
    const download::DownloadItem& item) {
  std::vector<DownloadSliceInfo> result;

  for (const auto& slice : item.GetReceivedSlices()) {
    result.emplace_back(item.GetId(), slice.offset, slice.received_bytes,
                        slice.finished);
  }

  return result;
}

}  // namespace history
