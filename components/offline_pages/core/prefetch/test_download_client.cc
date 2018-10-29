// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/test_download_client.h"

#include "components/offline_pages/core/prefetch/prefetch_downloader.h"

namespace offline_pages {

TestDownloadClient::TestDownloadClient(PrefetchDownloader* downloader)
    : downloader_(downloader) {}

void TestDownloadClient::OnDownloadFailed(
    const std::string& guid,
    const download::CompletionInfo& completion_info,
    download::Client::FailureReason reason) {
  downloader_->OnDownloadFailed(guid);
}

void TestDownloadClient::OnDownloadSucceeded(
    const std::string& guid,
    const download::CompletionInfo& completion_info) {
  downloader_->OnDownloadSucceeded(guid, completion_info.path,
                                   completion_info.bytes_downloaded);
}

}  // namespace offline_pages
