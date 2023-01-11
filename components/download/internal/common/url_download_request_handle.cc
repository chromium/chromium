// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/url_download_request_handle.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace download {

UrlDownloadRequestHandle::UrlDownloadRequestHandle(
    base::WeakPtr<UrlDownloadHandler> downloader,
    scoped_refptr<base::SequencedTaskRunner> downloader_task_runner)
    : downloader_(downloader),
      downloader_task_runner_(downloader_task_runner) {}

UrlDownloadRequestHandle::UrlDownloadRequestHandle(
    UrlDownloadRequestHandle&& other)
    : downloader_(std::move(other.downloader_)),
      downloader_task_runner_(std::move(other.downloader_task_runner_)) {}

UrlDownloadRequestHandle& UrlDownloadRequestHandle::operator=(
    UrlDownloadRequestHandle&& other) {
  downloader_ = std::move(other.downloader_);
  downloader_task_runner_ = std::move(other.downloader_task_runner_);
  return *this;
}

UrlDownloadRequestHandle::~UrlDownloadRequestHandle() = default;

}  // namespace download
