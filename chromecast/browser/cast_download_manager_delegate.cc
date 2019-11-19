// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_download_manager_delegate.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"

namespace chromecast {
namespace shell {

CastDownloadManagerDelegate::CastDownloadManagerDelegate() {}

CastDownloadManagerDelegate::~CastDownloadManagerDelegate() {}

void CastDownloadManagerDelegate::GetNextId(
      const content::DownloadIdCallback& callback) {
  // See default behavior of DownloadManagerImpl::GetNextId()
  static uint32_t next_id = download::DownloadItem::kInvalidId + 1;
  callback.Run(next_id++);
}

bool CastDownloadManagerDelegate::DetermineDownloadTarget(
    download::DownloadItem* item,
    const content::DownloadTargetCallback& callback) {
  base::FilePath empty;
  callback.Run(empty, download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT, empty,
               download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
  return true;
}

bool CastDownloadManagerDelegate::ShouldOpenFileBasedOnExtension(
    const base::FilePath& path) {
  return false;
}

bool CastDownloadManagerDelegate::ShouldCompleteDownload(
    download::DownloadItem* item,
    base::OnceClosure callback) {
  return false;
}

bool CastDownloadManagerDelegate::ShouldOpenDownload(
    download::DownloadItem* item,
    const content::DownloadOpenDelayedCallback& callback) {
  return false;
}

}  // namespace shell
}  // namespace chromecast
