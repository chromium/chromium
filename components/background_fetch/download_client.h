// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_FETCH_DOWNLOAD_CLIENT_H_
#define COMPONENTS_BACKGROUND_FETCH_DOWNLOAD_CLIENT_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/download/public/background_service/client.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace background_fetch {

class BackgroundFetchDelegateBase;

// A DownloadService client used by BackgroundFetch. Mostly this just forwards
// calls to BackgroundFetchDelegateBase.
class DownloadClient : public download::Client {
 public:
  // Create a client for |context|.
  explicit DownloadClient(content::BrowserContext* context);
  DownloadClient(const DownloadClient&) = delete;
  DownloadClient& operator=(const DownloadClient&) = delete;
  ~DownloadClient() override;

 private:
  BackgroundFetchDelegateBase* GetDelegate();

  // download::Client:
  void OnServiceInitialized(
      bool state_lost,
      const std::vector<download::DownloadMetaData>& downloads) override;
  void OnServiceUnavailable() override;
  void OnDownloadStarted(
      const std::string& guid,
      const std::vector<GURL>& url_chain,
      const scoped_refptr<const net::HttpResponseHeaders>& headers) override;
  void OnDownloadUpdated(const std::string& guid,
                         uint64_t bytes_uploaded,
                         uint64_t bytes_downloaded) override;
  void OnDownloadFailed(const std::string& guid,
                        const download::CompletionInfo& info,
                        download::Client::FailureReason reason) override;
  void OnDownloadSucceeded(const std::string& guid,
                           const download::CompletionInfo& info) override;
  bool CanServiceRemoveDownloadedFile(const std::string& guid,
                                      bool force_delete) override;
  void GetUploadData(const std::string& guid,
                     download::GetUploadDataCallback callback) override;

  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace background_fetch

#endif  // COMPONENTS_BACKGROUND_FETCH_DOWNLOAD_CLIENT_H_
