// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_MOJOM_TRAITS_H_

#include "chrome/browser/ui/webui/download_shelf/download_shelf.mojom.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"

namespace mojo {

template <>
struct EnumTraits<download_shelf::mojom::DangerType,
                  download::DownloadDangerType> {
  using DownloadDangerType = download::DownloadDangerType;
  using MojoDangerType = download_shelf::mojom::DangerType;

  static MojoDangerType ToMojom(DownloadDangerType input) {
    switch (input) {
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
        return MojoDangerType::kNotDangerous;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
        return MojoDangerType::kDangerousFile;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
        return MojoDangerType::kDangerousUrl;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
        return MojoDangerType::kDangerousContent;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
        return MojoDangerType::kMaybeDangerousContent;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
        return MojoDangerType::kUncommonContent;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
        return MojoDangerType::kUserValidated;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
        return MojoDangerType::kDangerousHost;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
        return MojoDangerType::kPotentiallyUnwanted;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
        return MojoDangerType::kAllowListedByPolicy;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
        return MojoDangerType::kAsyncScanning;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
        return MojoDangerType::kBlockedPasswordProtected;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
        return MojoDangerType::kBlockedTooLarge;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
        return MojoDangerType::kSensitiveContentWarning;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
        return MojoDangerType::kSensitiveContentBlock;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
        return MojoDangerType::kDeepScannedSafe;
      case DownloadDangerType::
          DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
        return MojoDangerType::kDeepScannedOpenedDangerous;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
        return MojoDangerType::kPromptForScanning;
      case DownloadDangerType::
          DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
        return MojoDangerType::kBlockedUnsupportedFileType;
      case DownloadDangerType::DOWNLOAD_DANGER_TYPE_MAX:
        break;
    }
    NOTREACHED();
    return MojoDangerType::kNotDangerous;
  }

  static bool FromMojom(MojoDangerType input, DownloadDangerType* out) {
    switch (input) {
      case MojoDangerType::kNotDangerous:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
        return true;
      case MojoDangerType::kDangerousFile:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
        return true;
      case MojoDangerType::kDangerousUrl:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL;
        return true;
      case MojoDangerType::kDangerousContent:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT;
        return true;
      case MojoDangerType::kMaybeDangerousContent:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
        return true;
      case MojoDangerType::kUncommonContent:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT;
        return true;
      case MojoDangerType::kUserValidated:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_USER_VALIDATED;
        return true;
      case MojoDangerType::kDangerousHost:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST;
        return true;
      case MojoDangerType::kPotentiallyUnwanted:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED;
        return true;
      case MojoDangerType::kAllowListedByPolicy:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY;
        return true;
      case MojoDangerType::kAsyncScanning:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING;
        return true;
      case MojoDangerType::kBlockedPasswordProtected:
        *out =
            DownloadDangerType::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED;
        return true;
      case MojoDangerType::kBlockedTooLarge:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE;
        return true;
      case MojoDangerType::kSensitiveContentWarning:
        *out =
            DownloadDangerType::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING;
        return true;
      case MojoDangerType::kSensitiveContentBlock:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK;
        return true;
      case MojoDangerType::kDeepScannedSafe:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE;
        return true;
      case MojoDangerType::kDeepScannedOpenedDangerous:
        *out = DownloadDangerType::
            DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS;
        return true;
      case MojoDangerType::kPromptForScanning:
        *out = DownloadDangerType::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING;
        return true;
      case MojoDangerType::kBlockedUnsupportedFileType:
        *out = DownloadDangerType::
            DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<download_shelf::mojom::DownloadState,
                  download::DownloadItem::DownloadState> {
  using DownloadState = download::DownloadItem::DownloadState;
  using MojoDownloadState = download_shelf::mojom::DownloadState;

  static MojoDownloadState ToMojom(DownloadState input) {
    switch (input) {
      case DownloadState::IN_PROGRESS:
        return MojoDownloadState::kInProgress;
      case DownloadState::COMPLETE:
        return MojoDownloadState::kComplete;
      case DownloadState::CANCELLED:
        return MojoDownloadState::kCancelled;
      case DownloadState::INTERRUPTED:
        return MojoDownloadState::kInterrupted;
      case DownloadState::MAX_DOWNLOAD_STATE:
        break;
    }
    NOTREACHED();
    return MojoDownloadState::kInProgress;
  }

  static bool FromMojom(MojoDownloadState input, DownloadState* out) {
    switch (input) {
      case MojoDownloadState::kInProgress:
        *out = DownloadState::IN_PROGRESS;
        return true;
      case MojoDownloadState::kComplete:
        *out = DownloadState::COMPLETE;
        return true;
      case MojoDownloadState::kCancelled:
        *out = DownloadState::CANCELLED;
        return true;
      case MojoDownloadState::kInterrupted:
        *out = DownloadState::INTERRUPTED;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<download_shelf::mojom::MixedContentStatus,
                  download::DownloadItem::MixedContentStatus> {
  using DownloadItemMixedContentStatus =
      download::DownloadItem::MixedContentStatus;
  using MojoMixedContentStatus = download_shelf::mojom::MixedContentStatus;

  static MojoMixedContentStatus ToMojom(DownloadItemMixedContentStatus input) {
    switch (input) {
      case DownloadItemMixedContentStatus::UNKNOWN:
        return MojoMixedContentStatus::kUnknown;
      case DownloadItemMixedContentStatus::SAFE:
        return MojoMixedContentStatus::kSafe;
      case DownloadItemMixedContentStatus::VALIDATED:
        return MojoMixedContentStatus::kValidated;
      case DownloadItemMixedContentStatus::WARN:
        return MojoMixedContentStatus::kWarn;
      case DownloadItemMixedContentStatus::BLOCK:
        return MojoMixedContentStatus::kBlock;
      case DownloadItemMixedContentStatus::SILENT_BLOCK:
        return MojoMixedContentStatus::kSilentBlock;
    }
    NOTREACHED();
    return MojoMixedContentStatus::kUnknown;
  }

  static bool FromMojom(MojoMixedContentStatus input,
                        DownloadItemMixedContentStatus* out) {
    switch (input) {
      case MojoMixedContentStatus::kUnknown:
        *out = DownloadItemMixedContentStatus::UNKNOWN;
        return true;
      case MojoMixedContentStatus::kSafe:
        *out = DownloadItemMixedContentStatus::SAFE;
        return true;
      case MojoMixedContentStatus::kValidated:
        *out = DownloadItemMixedContentStatus::VALIDATED;
        return true;
      case MojoMixedContentStatus::kWarn:
        *out = DownloadItemMixedContentStatus::WARN;
        return true;
      case MojoMixedContentStatus::kBlock:
        *out = DownloadItemMixedContentStatus::BLOCK;
        return true;
      case MojoMixedContentStatus::kSilentBlock:
        *out = DownloadItemMixedContentStatus::SILENT_BLOCK;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_MOJOM_TRAITS_H_
