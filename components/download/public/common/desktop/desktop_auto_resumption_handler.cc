// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/desktop/desktop_auto_resumption_handler.h"

#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/download/public/common/download_item_impl.h"

namespace download {

// static
DesktopAutoResumptionHandler* DesktopAutoResumptionHandler::Get() {
  static base::NoDestructor<DesktopAutoResumptionHandler> handler;
  return handler.get();
}

DesktopAutoResumptionHandler::DesktopAutoResumptionHandler() {}

DesktopAutoResumptionHandler::~DesktopAutoResumptionHandler() = default;

base::TimeDelta DesktopAutoResumptionHandler::ComputeBackoffDelay(
    int retry_count) {
  // Implement exponential backoff algorithm
  const base::TimeDelta kInitialDelay = base::Milliseconds(500);

  int64_t delay = kInitialDelay.InMilliseconds() * (1 << retry_count);
  // Add random jitter of +/- 15% of the calculated delay to avoid thundering
  // herd
  int64_t jitter_ms = (delay * 15) / 100;  // 15% of delay
  delay += base::RandInt(-jitter_ms, jitter_ms);

  return base::Milliseconds(delay);
}

void DesktopAutoResumptionHandler::OnDownloadUpdated(DownloadItem* item) {
  if (item->GetState() == DownloadItem::INTERRUPTED &&
      IsAutoResumableDownload(item)) {
    resumable_downloads_[item->GetGuid()] = item;
    base::TimeDelta delay = ComputeBackoffDelay(item->GetAutoResumeCount());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DesktopAutoResumptionHandler::MaybeResumeDownload,
                       weak_factory_.GetWeakPtr(), item->GetGuid()),
        delay);
  }
}

void DesktopAutoResumptionHandler::OnDownloadDestroyed(DownloadItem* item) {
  item->RemoveObserver(this);
  resumable_downloads_.erase(item->GetGuid());
}

bool DesktopAutoResumptionHandler::IsAutoResumableDownload(
    DownloadItem* item) const {
  if (!item) {
    return false;
  }
  switch (item->GetState()) {
    case DownloadItem::IN_PROGRESS:
      return !item->IsPaused();
    case DownloadItem::COMPLETE:
    case DownloadItem::CANCELLED:
      return false;
    case DownloadItem::INTERRUPTED:
      return !item->IsPaused();
    case DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }

  return false;
}

void DesktopAutoResumptionHandler::MaybeResumeDownload(std::string guid) {
  // Check if the download is still resumable
  if (resumable_downloads_.find(guid) == resumable_downloads_.end()) {
    return;
  }

  auto item = resumable_downloads_[guid];
  if (!IsAutoResumableDownload(item)) {
    resumable_downloads_.erase(guid);
    return;
  }

  item->Resume(false);
}

}  // namespace download
