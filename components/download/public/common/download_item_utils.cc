// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_item_utils.h"

#include "chromeos/crosapi/mojom/download_controller.mojom.h"

namespace download {
namespace download_item_utils {
namespace {

crosapi::mojom::DownloadDangerType ConvertToMojoDownloadDangerType(
    DownloadDangerType value) {
  switch (value) {
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeNotDangerous;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeDangerousFile;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeDangerousUrl;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeDangerousContent;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeMaybeDangerousContent;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeUncommonContent;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeUserValidated;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeDangerousHost;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypePotentiallyUnwanted;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeAllowlistedByPolicy;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeAsyncScanning;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeAsyncLocalPasswordScanning;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeBlockedPasswordProtected;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeBlockedTooLarge;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeSensitiveContentWarning;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeSensitiveContentBlock;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeDeepScannedSafe;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeDeepScannedOpenedDangerous;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypePromptForScanning;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeDangerousAccountCompromise;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeDeepScannedFailed;
    case DownloadDangerType::
        DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypePromptForLocalPasswordScanning;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeBlockedScanFailed;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED_IN_MIGRATION();
      return crosapi::mojom::DownloadDangerType::kDownloadDangerTypeInvalid;
  }
}

crosapi::mojom::InsecureDownloadStatus ConvertToMojoInsecureDownloadStatus(
    DownloadItem::InsecureDownloadStatus value) {
  switch (value) {
    case DownloadItem::InsecureDownloadStatus::UNKNOWN:
      return crosapi::mojom::InsecureDownloadStatus::kUnknown;
    case DownloadItem::InsecureDownloadStatus::SAFE:
      return crosapi::mojom::InsecureDownloadStatus::kSafe;
    case DownloadItem::InsecureDownloadStatus::VALIDATED:
      return crosapi::mojom::InsecureDownloadStatus::kValidated;
    case DownloadItem::InsecureDownloadStatus::WARN:
      return crosapi::mojom::InsecureDownloadStatus::kWarn;
    case DownloadItem::InsecureDownloadStatus::BLOCK:
      return crosapi::mojom::InsecureDownloadStatus::kBlock;
    case DownloadItem::InsecureDownloadStatus::SILENT_BLOCK:
      return crosapi::mojom::InsecureDownloadStatus::kSilentBlock;
  }
}

}  // namespace

crosapi::mojom::DownloadItemPtr ConvertToMojoDownloadItem(
    const DownloadItem* item,
    bool is_from_incognito_profile) {
  auto download = crosapi::mojom::DownloadItem::New();
  download->guid = item->GetGuid();
  download->state = ConvertToMojoDownloadState(item->GetState());
  download->full_path = item->GetFullPath();
  download->target_file_path = item->GetTargetFilePath();
  download->is_from_incognito_profile = is_from_incognito_profile;
  download->is_paused = item->IsPaused();
  download->has_is_paused = true;
  download->open_when_complete = item->GetOpenWhenComplete();
  download->has_open_when_complete = true;
  download->received_bytes = item->GetReceivedBytes();
  download->has_received_bytes = true;
  download->total_bytes = item->GetTotalBytes();
  download->has_total_bytes = true;
  download->start_time = item->GetStartTime();
  download->is_dangerous = item->IsDangerous();
  download->has_is_dangerous = true;
  download->is_insecure = item->IsInsecure();
  download->has_is_insecure = true;
  download->danger_type =
      ConvertToMojoDownloadDangerType(item->GetDangerType());
  download->insecure_download_status =
      ConvertToMojoInsecureDownloadStatus(item->GetInsecureDownloadStatus());
  return download;
}

crosapi::mojom::DownloadState ConvertToMojoDownloadState(
    DownloadItem::DownloadState state) {
  switch (state) {
    case DownloadItem::IN_PROGRESS:
      return crosapi::mojom::DownloadState::kInProgress;
    case DownloadItem::COMPLETE:
      return crosapi::mojom::DownloadState::kComplete;
    case DownloadItem::CANCELLED:
      return crosapi::mojom::DownloadState::kCancelled;
    case DownloadItem::INTERRUPTED:
      return crosapi::mojom::DownloadState::kInterrupted;
    case DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }
}

}  // namespace download_item_utils
}  // namespace download
