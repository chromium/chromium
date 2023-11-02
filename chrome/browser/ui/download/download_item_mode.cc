// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_item_mode.h"

#include "chrome/browser/download/download_ui_model.h"

namespace download {

DownloadItemMode GetDesiredDownloadItemMode(DownloadUIModel* download) {
  if (download->IsMixedContent()) {
    const bool warn = download->GetMixedContentStatus() ==
                      download::DownloadItem::MixedContentStatus::WARN;
    return warn ? DownloadItemMode::kMixedContentWarn
                : DownloadItemMode::kMixedContentBlock;
  }

  if (download->IsDangerous() &&
      (download->GetState() != download::DownloadItem::CANCELLED)) {
    return download->MightBeMalicious() ? DownloadItemMode::kMalicious
                                        : DownloadItemMode::kDangerous;
  }

  return ((download->GetDangerType() ==
           download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING) &&
          (download->GetState() != download::DownloadItem::CANCELLED))
             ? DownloadItemMode::kDeepScanning
             : DownloadItemMode::kNormal;
}

}  // namespace download
