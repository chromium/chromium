// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_WORKER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_WORKER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_request_handle_interface.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/url_download_handler.h"

namespace net {
class URLRequestContextGetter;
}

namespace download {
class DownloadURLLoaderFactoryGetter;

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

  DownloadWorker(DownloadWorker::Delegate* delegate,
                 int64_t offset,
                 int64_t length);
  virtual ~DownloadWorker();

  int64_t offset() const { return offset_; }
  int64_t length() const { return length_; }

  // Send network request to ask for a download.
  void SendRequest(
      std::unique_ptr<DownloadUrlParameters> params,
      scoped_refptr<download::DownloadURLLoaderFactoryGetter>
          url_loader_factory_getter,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

  // Download operations.
  void Pause();
  void Resume();
  void Cancel(bool user_cancel);

 private:
  // UrlDownloader::Delegate implementation.
  void OnUrlDownloadStarted(
      std::unique_ptr<DownloadCreateInfo> create_info,
      std::unique_ptr<InputStream> input_stream,
      scoped_refptr<download::DownloadURLLoaderFactoryGetter>
          url_loader_factory_getter,
      const DownloadUrlParameters::OnStartedCallback& callback) override;
  void OnUrlDownloadStopped(UrlDownloadHandler* downloader) override;
  void OnUrlDownloadHandlerCreated(
      UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader) override;

  DownloadWorker::Delegate* const delegate_;

  // The starting position of the content for this worker to download.
  int64_t offset_;

  // The length of the request. May be 0 to fetch to the end of the file.
  int64_t length_;

  // States of the worker.
  bool is_paused_;
  bool is_canceled_;
  bool is_user_cancel_;

  // Used to control the network request. Live on UI thread.
  std::unique_ptr<DownloadRequestHandleInterface> request_handle_;

  // Used to handle the url request. Live and die on IO thread.
  UrlDownloadHandler::UniqueUrlDownloadHandlerPtr url_download_handler_;

  base::WeakPtrFactory<DownloadWorker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadWorker);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_WORKER_H_
