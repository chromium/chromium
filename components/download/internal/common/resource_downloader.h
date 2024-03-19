// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_RESOURCE_DOWNLOADER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_RESOURCE_DOWNLOADER_H_

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_response_handler.h"
#include "components/download/public/common/download_utils.h"
#include "components/download/public/common/url_download_handler.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_status_flags.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace download {
// Class for handing the download of a url. Lives on IO thread.
class COMPONENTS_DOWNLOAD_EXPORT ResourceDownloader
    : public UrlDownloadHandler,
      public DownloadResponseHandler::Delegate {
 public:
  // Called to start a download, must be called on IO thread.
  static std::unique_ptr<ResourceDownloader> BeginDownload(
      base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
      std::unique_ptr<download::DownloadUrlParameters> download_url_parameters,
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const URLSecurityPolicy& url_security_policy,
      const std::string& serialized_embedder_download_data,
      const GURL& tab_url,
      const GURL& tab_referrer_url,
      bool is_new_download,
      bool is_parallel_request,
      mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider,
      bool is_background_mode,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Create a ResourceDownloader from a navigation that turns to be a download.
  // No URLLoader is created, but the URLLoaderClient implementation is
  // transferred.
  static void InterceptNavigationResponse(
      base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
      std::unique_ptr<network::ResourceRequest> resource_request,
      int render_process_id,
      int render_frame_id,
      const std::string& serialized_embedder_download_data,
      const GURL& tab_url,
      const GURL& tab_referrer_url,
      std::vector<GURL> url_chain,
      net::CertStatus cert_status,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const URLSecurityPolicy& url_security_policy,
      mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      bool is_transient);

  ResourceDownloader(
      base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
      std::unique_ptr<network::ResourceRequest> resource_request,
      int render_process_id,
      int render_frame_id,
      const std::string& serialized_embedder_download_data,
      const GURL& tab_url,
      const GURL& tab_referrer_url,
      bool is_new_download,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const URLSecurityPolicy& url_security_policy,
      mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider);

  ResourceDownloader(const ResourceDownloader&) = delete;
  ResourceDownloader& operator=(const ResourceDownloader&) = delete;

  ~ResourceDownloader() override;

  // DownloadResponseHandler::Delegate
  void OnResponseStarted(
      std::unique_ptr<download::DownloadCreateInfo> download_create_info,
      mojom::DownloadStreamHandlePtr stream_handle) override;
  void OnReceiveRedirect() override;
  void OnResponseCompleted() override;
  bool CanRequestURL(const GURL& url) override;
  void OnUploadProgress(uint64_t bytes_uploaded) override;

 private:
  // Helper method to start the network request.
  void Start(
      std::unique_ptr<download::DownloadUrlParameters> download_url_parameters,
      bool is_parallel_request,
      bool is_background_mode);

  // Intercepts the navigation response.
  void InterceptResponse(
      std::vector<GURL> url_chain,
      net::CertStatus cert_status,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      bool is_transient);

  // Ask the |delegate_| to destroy this object.
  void Destroy();

  // Requests the wake lock using |provider|.
  void RequestWakeLock(device::mojom::WakeLockProvider* provider);

  base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate_;

  // The ResourceRequest for this object.
  std::unique_ptr<network::ResourceRequest> resource_request_;

  // Object that will handle the response.
  std::unique_ptr<network::mojom::URLLoaderClient> url_loader_client_;

  // URLLoaderClient receiver. It sends any requests to the
  // |url_loader_client_|.
  std::unique_ptr<mojo::Receiver<network::mojom::URLLoaderClient>>
      url_loader_client_receiver_;

  // URLLoader for sending out the request.
  mojo::Remote<network::mojom::URLLoader> url_loader_;

  // Whether this is a new download.
  bool is_new_download_;

  // GUID of the download, or empty if this is a new download.
  std::string guid_;

  // Callback to run after download starts.
  download::DownloadUrlParameters::OnStartedCallback callback_;

  // Callback to run with upload updates.
  DownloadUrlParameters::UploadProgressCallback upload_callback_;

  // Frame and process id associated with the request.
  int render_process_id_;
  int render_frame_id_;

  // Serialized embedder data download for the site instance that initiated the
  // download.
  std::string serialized_embedder_download_data_;

  // The URL of the tab that started us.
  GURL tab_url_;

  // The referrer URL of the tab that started us.
  GURL tab_referrer_url_;

  // URLLoader status when intercepting the navigation request.
  std::optional<network::URLLoaderCompletionStatus> url_loader_status_;

  // TaskRunner to post callbacks to the |delegate_|
  scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;

  // URLLoaderFactory for issuing network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Used to check if the URL is safe to request.
  URLSecurityPolicy url_security_policy_;

  // Whether download is initiated by the content on the page.
  bool is_content_initiated_;

#if BUILDFLAG(IS_ANDROID)
  // Whether the original URL must be downloaded.
  bool is_must_download_ = true;
#endif  // BUILDFLAG(IS_ANDROID)

  // Used to keep the system from sleeping while a download is ongoing. If the
  // system enters power saving mode while a download is alive, it can cause
  // download to be interrupted.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_RESOURCE_DOWNLOADER_H_
