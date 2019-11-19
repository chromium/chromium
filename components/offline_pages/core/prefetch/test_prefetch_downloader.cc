// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/test_prefetch_downloader.h"

namespace offline_pages {

TestPrefetchDownloader::TestPrefetchDownloader() = default;

TestPrefetchDownloader::~TestPrefetchDownloader() = default;

void TestPrefetchDownloader::SetPrefetchService(PrefetchService* service) {}

bool TestPrefetchDownloader::IsDownloadServiceUnavailable() const {
  return false;
}

void TestPrefetchDownloader::CleanupDownloadsWhenReady() {}

void TestPrefetchDownloader::StartDownload(const std::string& download_id,
                                           const std::string& download_location,
                                           const std::string& operation_name) {
  Request request;
  request.download_location = download_location;
  request.operation_name = operation_name;
  requested_downloads_[download_id] = request;
}

void TestPrefetchDownloader::OnDownloadServiceReady(
    const std::set<std::string>& outstanding_download_ids,
    const std::map<std::string, std::pair<base::FilePath, int64_t>>&
        success_downloads) {}

void TestPrefetchDownloader::OnDownloadServiceUnavailable() {}

void TestPrefetchDownloader::OnDownloadSucceeded(
    const std::string& download_id,
    const base::FilePath& file_path,
    int64_t file_size) {}

void TestPrefetchDownloader::OnDownloadFailed(const std::string& download_id) {}

int TestPrefetchDownloader::GetMaxConcurrentDownloads() {
  return max_concurrent_downloads_;
}

void TestPrefetchDownloader::Reset() {
  requested_downloads_.clear();
}

}  // namespace offline_pages
