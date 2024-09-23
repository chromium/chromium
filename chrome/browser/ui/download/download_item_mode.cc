// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_item_mode.h"

#include "chrome/browser/download/download_ui_model.h"

namespace download {

DownloadItemMode GetDesiredDownloadItemMode(const DownloadUIModel* download) {
  if (download->IsInsecure()) {
    const bool warn = download->GetInsecureDownloadStatus() ==
                      download::DownloadItem::InsecureDownloadStatus::WARN;
    return warn ? DownloadItemMode::kInsecureDownloadWarn
                : DownloadItemMode::kInsecureDownloadBlock;
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
