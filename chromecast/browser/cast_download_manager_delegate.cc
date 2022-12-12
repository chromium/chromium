// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_download_manager_delegate.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"

namespace chromecast {
namespace shell {

CastDownloadManagerDelegate::CastDownloadManagerDelegate() {}

CastDownloadManagerDelegate::~CastDownloadManagerDelegate() {}

void CastDownloadManagerDelegate::GetNextId(
    content::DownloadIdCallback callback) {
  // See default behavior of DownloadManagerImpl::GetNextId()
  static uint32_t next_id = download::DownloadItem::kInvalidId + 1;
  std::move(callback).Run(next_id++);
}

bool CastDownloadManagerDelegate::DetermineDownloadTarget(
    download::DownloadItem* item,
    content::DownloadTargetCallback* callback) {
  base::FilePath empty;
  std::move(*callback).Run(
      empty, download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      download::DownloadItem::InsecureDownloadStatus::UNKNOWN, empty, empty,
      std::string() /*mime_type*/,
      download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
  return true;
}

bool CastDownloadManagerDelegate::ShouldCompleteDownload(
    download::DownloadItem* item,
    base::OnceClosure callback) {
  return false;
}

bool CastDownloadManagerDelegate::ShouldOpenDownload(
    download::DownloadItem* item,
    content::DownloadOpenDelayedCallback callback) {
  // TODO(qinmin): When this returns false it means this should run the callback
  // at some point.
  return false;
}

}  // namespace shell
}  // namespace chromecast
