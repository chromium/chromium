// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {
class PrefetchService;
static constexpr base::TimeDelta kPrefetchDownloadLifetime =
    base::TimeDelta::FromDays(2);

// Asynchronously downloads the archive.
class PrefetchDownloader {
 public:
  virtual ~PrefetchDownloader() = default;

  virtual void SetPrefetchService(PrefetchService* service) = 0;

  // Returned true if the download service is not available and can't be used.
  virtual bool IsDownloadServiceUnavailable() const = 0;

  // Notifies that the download cleanup can be triggered immediately when the
  // download service is ready. If the download service is ready before this
  // method is called, the download cleanup should be delayed.
  virtual void CleanupDownloadsWhenReady() = 0;

  // Starts to download an archive from |download_location|.
  virtual void StartDownload(const std::string& download_id,
                             const std::string& download_location,
                             const std::string& operation_name) = 0;

  // Called when the download service is initialized.
  // |success_downloads| is a map with download_id as key and pair of file path
  // and file size as value.
  virtual void OnDownloadServiceReady(
      const std::set<std::string>& outstanding_download_ids,
      const std::map<std::string, std::pair<base::FilePath, int64_t>>&
          success_downloads) = 0;

  // Called when the download service fails to initialize and should not be
  // used.
  virtual void OnDownloadServiceUnavailable() = 0;

  // Called when a download is completed successfully. Note that the download
  // can be scheduled in previous sessions.
  virtual void OnDownloadSucceeded(const std::string& download_id,
                                   const base::FilePath& file_path,
                                   int64_t file_size) = 0;

  // Called when a download fails.
  virtual void OnDownloadFailed(const std::string& download_id) = 0;

  // Returns the maximum number of downloads allowed.
  virtual int GetMaxConcurrentDownloads() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DOWNLOADER_H_
