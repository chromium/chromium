// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_RESPONSE_HANDLER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_RESPONSE_HANDLER_H_

#include <string>
#include <vector>

#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_source.h"
#include "components/download/public/common/download_stream.mojom.h"
#include "components/download/public/common/download_url_parameters.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace download {

// This class is responsible for handling the server response for a download.
// It passes the DataPipeConsumerHandle and completion status to the download
// sink. The class is common to both navigation triggered downloads and
// context menu downloads
class COMPONENTS_DOWNLOAD_EXPORT DownloadResponseHandler
    : public network::mojom::URLLoaderClient {
 public:
  // Class for handling the stream response.
  class Delegate {
   public:
    virtual void OnResponseStarted(
        std::unique_ptr<DownloadCreateInfo> download_create_info,
        mojom::DownloadStreamHandlePtr stream_handle) = 0;
    virtual void OnReceiveRedirect() = 0;
    virtual void OnResponseCompleted() = 0;
  };

  DownloadResponseHandler(
      network::ResourceRequest* resource_request,
      Delegate* delegate,
      std::unique_ptr<DownloadSaveInfo> save_info,
      bool is_parallel_request,
      bool is_transient,
      bool fetch_error_body,
      bool follow_cross_origin_redirects,
      const DownloadUrlParameters::RequestHeadersType& request_headers,
      const std::string& request_origin,
      DownloadSource download_source,
      std::vector<GURL> url_chain);
  ~DownloadResponseHandler() override;

  // network::mojom::URLLoaderClient
  void OnReceiveResponse(const network::ResourceResponseHead& head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  std::unique_ptr<DownloadCreateInfo> CreateDownloadCreateInfo(
      const network::ResourceResponseHead& head);

  // Helper method that is called when response is received.
  void OnResponseStarted(mojom::DownloadStreamHandlePtr stream_handle);

  Delegate* const delegate_;

  std::unique_ptr<DownloadCreateInfo> create_info_;

  bool started_;

  // Information needed to create DownloadCreateInfo when the time comes.
  std::unique_ptr<DownloadSaveInfo> save_info_;
  std::vector<GURL> url_chain_;
  std::string method_;
  GURL referrer_;
  net::URLRequest::ReferrerPolicy referrer_policy_;
  bool is_transient_;
  bool fetch_error_body_;
  bool follow_cross_origin_redirects_;
  url::Origin first_origin_;
  DownloadUrlParameters::RequestHeadersType request_headers_;
  std::string request_origin_;
  DownloadSource download_source_;
  net::CertStatus cert_status_;
  bool has_strong_validators_;
  GURL origin_;
  bool is_partial_request_;
  bool completed_;

  // The abort reason if this class decides to block the download.
  DownloadInterruptReason abort_reason_;

  // Mojo interface ptr to send the completion status to the download sink.
  mojom::DownloadStreamClientPtr client_ptr_;

  DISALLOW_COPY_AND_ASSIGN(DownloadResponseHandler);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_RESPONSE_HANDLER_H_
