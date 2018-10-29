// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_DOWNLOAD_CLIENT_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_DOWNLOAD_CLIENT_H_

#include "base/macros.h"
#include "components/download/public/background_service/test/empty_client.h"

namespace offline_pages {

class PrefetchDownloader;

class TestDownloadClient : public download::test::EmptyClient {
 public:
  explicit TestDownloadClient(PrefetchDownloader* downloader);
  ~TestDownloadClient() override = default;

  void OnDownloadFailed(const std::string& guid,
                        const download::CompletionInfo& completion_info,
                        download::Client::FailureReason reason) override;
  void OnDownloadSucceeded(
      const std::string& guid,
      const download::CompletionInfo& completion_info) override;

 private:
  PrefetchDownloader* downloader_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloadClient);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_DOWNLOAD_CLIENT_H_
