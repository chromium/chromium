// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_DOWNLOADER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_DOWNLOADER_H_

#include <map>
#include <string>

#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

// Mock implementation of prefetch downloader that is suitable for testing.
class TestPrefetchDownloader : public PrefetchDownloader {
 public:
  struct Request {
    std::string download_location;
    std::string operation_name;
  };
  using RequestMap = std::map<std::string, Request>;
  TestPrefetchDownloader();
  ~TestPrefetchDownloader() override;

  void SetPrefetchService(PrefetchService* service) override;
  bool IsDownloadServiceUnavailable() const override;
  void CleanupDownloadsWhenReady() override;
  void StartDownload(const std::string& download_id,
                     const std::string& download_location,
                     const std::string& operation_name) override;
  void OnDownloadServiceReady(
      const std::set<std::string>& outstanding_download_ids,
      const std::map<std::string, std::pair<base::FilePath, int64_t>>&
          success_downloads) override;
  void OnDownloadServiceUnavailable() override;
  void OnDownloadSucceeded(const std::string& download_id,
                           const base::FilePath& file_path,
                           int64_t file_size) override;
  void OnDownloadFailed(const std::string& download_id) override;
  int GetMaxConcurrentDownloads() override;

  void Reset();

  const RequestMap& requested_downloads() const { return requested_downloads_; }

  void SetMaxConcurrentDownloads(int max_concurrent_downloads) {
    max_concurrent_downloads_ = max_concurrent_downloads;
  }

 private:
  RequestMap requested_downloads_;
  int max_concurrent_downloads_ = 4;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_DOWNLOADER_H_
