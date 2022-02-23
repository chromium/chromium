// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_item_utils.h"

#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "components/download/public/common/download_item.h"

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
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeBlockedUnsupportedFiletype;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return crosapi::mojom::DownloadDangerType::
          kDownloadDangerTypeDangerousAccountCompromise;
    case DownloadDangerType::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED();
      return crosapi::mojom::DownloadDangerType::kDownloadDangerTypeInvalid;
  }
}

crosapi::mojom::DownloadMixedContentStatus
ConvertToMojoDownloadMixedContentStatus(
    DownloadItem::MixedContentStatus value) {
  switch (value) {
    case DownloadItem::MixedContentStatus::UNKNOWN:
      return crosapi::mojom::DownloadMixedContentStatus::kUnknown;
    case DownloadItem::MixedContentStatus::SAFE:
      return crosapi::mojom::DownloadMixedContentStatus::kSafe;
    case DownloadItem::MixedContentStatus::VALIDATED:
      return crosapi::mojom::DownloadMixedContentStatus::kValidated;
    case DownloadItem::MixedContentStatus::WARN:
      return crosapi::mojom::DownloadMixedContentStatus::kWarn;
    case DownloadItem::MixedContentStatus::BLOCK:
      return crosapi::mojom::DownloadMixedContentStatus::kBlock;
    case DownloadItem::MixedContentStatus::SILENT_BLOCK:
      return crosapi::mojom::DownloadMixedContentStatus::kSilentBlock;
  }
}

crosapi::mojom::DownloadState ConvertToMojoDownloadState(
    DownloadItem::DownloadState value) {
  switch (value) {
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
      return crosapi::mojom::DownloadState::kUnknown;
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
  download->is_mixed_content = item->IsMixedContent();
  download->has_is_mixed_content = true;
  download->danger_type =
      ConvertToMojoDownloadDangerType(item->GetDangerType());
  download->mixed_content_status =
      ConvertToMojoDownloadMixedContentStatus(item->GetMixedContentStatus());
  return download;
}

}  // namespace download_item_utils
}  // namespace download
