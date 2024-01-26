// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_download_manager_delegate.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_target_info.h"

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
    download::DownloadTargetCallback* callback) {
  download::DownloadTargetInfo target_info;
  target_info.danger_type =
      download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;
  target_info.interrupt_reason =
      download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;

  std::move(*callback).Run(std::move(target_info));
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
