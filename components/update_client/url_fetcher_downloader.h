// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_URL_FETCHER_DOWNLOADER_H_
#define COMPONENTS_UPDATE_CLIENT_URL_FETCHER_DOWNLOADER_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/update_client/crx_downloader.h"

namespace update_client {

class NetworkFetcher;
class NetworkFetcherFactory;

// Implements a CRX downloader using a NetworkFetcher object.
class UrlFetcherDownloader : public CrxDownloader {
 public:
  UrlFetcherDownloader(
      scoped_refptr<CrxDownloader> successor,
      scoped_refptr<NetworkFetcherFactory> network_fetcher_factory);
  UrlFetcherDownloader(const UrlFetcherDownloader&) = delete;
  UrlFetcherDownloader& operator=(const UrlFetcherDownloader&) = delete;

 private:
  // Overrides for CrxDownloader.
  ~UrlFetcherDownloader() override;
  base::OnceClosure DoStartDownload(const GURL& url) override;

  void CreateDownloadDir();
  void StartURLFetch(const GURL& url);
  void OnNetworkFetcherComplete(int net_error, int64_t content_size);
  void OnResponseStarted(int response_code, int64_t content_length);
  void OnDownloadProgress(int64_t content_length);
  void Cancel();

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<NetworkFetcherFactory> network_fetcher_factory_;
  std::unique_ptr<NetworkFetcher> network_fetcher_;

  // Contains a temporary download directory for the downloaded file.
  base::FilePath download_dir_;

  // Contains the file path to the downloaded file.
  base::FilePath file_path_;

  base::TimeTicks download_start_time_;

  base::OnceClosure cancel_callback_;
  bool cancelled_ = false;

  int response_code_ = -1;
  int64_t total_bytes_ = -1;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_URL_FETCHER_DOWNLOADER_H_
