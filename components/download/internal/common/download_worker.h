// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_WORKER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_WORKER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/url_download_handler.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace download {

// Helper class used to send subsequent range requests to fetch slices of the
// file after handling response of the original non-range request.
// TODO(xingliu): we should consider to reuse this class for single connection
// download.
class COMPONENTS_DOWNLOAD_EXPORT DownloadWorker
    : public UrlDownloadHandler::Delegate {
 public:
  class Delegate {
   public:
    // Called when the the input stream is established after server response is
    // handled. The stream contains data starts from |offset| of the
    // destination file.
    virtual void OnInputStreamReady(
        DownloadWorker* worker,
        std::unique_ptr<InputStream> input_stream,
        std::unique_ptr<DownloadCreateInfo> download_create_info) = 0;
  };

  DownloadWorker(DownloadWorker::Delegate* delegate, int64_t offset);

  DownloadWorker(const DownloadWorker&) = delete;
  DownloadWorker& operator=(const DownloadWorker&) = delete;

  virtual ~DownloadWorker();

  int64_t offset() const { return offset_; }

  // Send network request to ask for a download.
  void SendRequest(
      std::unique_ptr<DownloadUrlParameters> params,
      URLLoaderFactoryProvider* url_loader_factory_provider,
      mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider);

  // Download operations.
  void Pause();
  void Resume();
  void Cancel(bool user_cancel);

 private:
  // UrlDownloader::Delegate implementation.
  void OnUrlDownloadStarted(
      std::unique_ptr<DownloadCreateInfo> create_info,
      std::unique_ptr<InputStream> input_stream,
      URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
          url_loader_factory_provider,
      UrlDownloadHandlerID downloader,
      DownloadUrlParameters::OnStartedCallback callback) override;
  void OnUrlDownloadStopped(UrlDownloadHandlerID downloader) override;
  void OnUrlDownloadHandlerCreated(
      UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader) override;

  const raw_ptr<DownloadWorker::Delegate> delegate_;

  // The starting position of the content for this worker to download.
  int64_t offset_;

  // States of the worker.
  bool is_paused_;
  bool is_canceled_;

  // Used to handle the url request. Live and die on IO thread.
  UrlDownloadHandler::UniqueUrlDownloadHandlerPtr url_download_handler_;

  base::WeakPtrFactory<DownloadWorker> weak_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_WORKER_H_
