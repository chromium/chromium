// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_job.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_task_runner.h"

namespace download {

DownloadJob::DownloadJob(DownloadItem* download_item,
                         CancelRequestCallback cancel_request_callback)
    : download_item_(download_item),
      cancel_request_callback_(std::move(cancel_request_callback)),
      is_paused_(false) {}

DownloadJob::~DownloadJob() = default;

void DownloadJob::Cancel(bool user_cancel) {
  if (cancel_request_callback_)
    std::move(cancel_request_callback_).Run(user_cancel);
}

void DownloadJob::Pause() {
  is_paused_ = true;

  DownloadFile* download_file = download_item_->GetDownloadFile();
  if (download_file) {
    GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadFile::Pause,
                       // Safe because we control download file lifetime.
                       base::Unretained(download_file)));
  }
}

void DownloadJob::Resume(bool resume_request) {
  is_paused_ = false;
  if (!resume_request)
    return;

  DownloadFile* download_file = download_item_->GetDownloadFile();
  if (download_file) {
    GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadFile::Resume,
                       // Safe because we control download file lifetime.
                       base::Unretained(download_file)));
  }
}

void DownloadJob::Start(DownloadFile* download_file_,
                        DownloadFile::InitializeCallback callback,
                        const DownloadItem::ReceivedSlices& received_slices) {
  GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DownloadFile::Initialize,
          // Safe because we control download file lifetime.
          base::Unretained(download_file_),
          base::BindRepeating(&DownloadJob::OnDownloadFileInitialized,
                              weak_ptr_factory_.GetWeakPtr(), callback),
          base::BindRepeating(&DownloadJob::CancelRequestWithOffset,
                              weak_ptr_factory_.GetWeakPtr()),
          received_slices, IsParallelizable()));
}

void DownloadJob::OnDownloadFileInitialized(
    DownloadFile::InitializeCallback callback,
    DownloadInterruptReason result,
    int64_t bytes_wasted) {
  std::move(callback).Run(result, bytes_wasted);
}

bool DownloadJob::AddInputStream(std::unique_ptr<InputStream> stream,
                                 int64_t offset) {
  DownloadFile* download_file = download_item_->GetDownloadFile();
  if (!download_file) {
    CancelRequestWithOffset(offset);
    return false;
  }

  // download_file_ is owned by download_item_ on the UI thread and is always
  // deleted on the download task runner after download_file_ is nulled out.
  // So it's safe to use base::Unretained here.
  GetDownloadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&DownloadFile::AddInputStream,
                                base::Unretained(download_file),
                                std::move(stream), offset));
  return true;
}

void DownloadJob::CancelRequestWithOffset(int64_t offset) {}

bool DownloadJob::IsParallelizable() const {
  return false;
}

bool DownloadJob::IsSavePackageDownload() const {
  return false;
}

}  // namespace download
