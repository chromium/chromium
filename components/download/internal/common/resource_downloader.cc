// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/resource_downloader.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_utils.h"
#include "components/download/public/common/stream_handle_input_stream.h"
#include "components/download/public/common/url_download_handler.h"
#include "components/download/public/common/url_loader_factory_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace download {

// This object monitors the URLLoaderCompletionStatus change when
// ResourceDownloader is asking |delegate_| whether download can proceed.
class URLLoaderStatusMonitor : public network::mojom::URLLoaderClient {
 public:
  using URLLoaderStatusChangeCallback =
      base::OnceCallback<void(const network::URLLoaderCompletionStatus&)>;
  explicit URLLoaderStatusMonitor(URLLoaderStatusChangeCallback callback);

  URLLoaderStatusMonitor(const URLLoaderStatusMonitor&) = delete;
  URLLoaderStatusMonitor& operator=(const URLLoaderStatusMonitor&) = delete;

  ~URLLoaderStatusMonitor() override = default;

  // network::mojom::URLLoaderClient
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {}
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {}
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    network::RecordOnTransferSizeUpdatedUMA(
        network::OnTransferSizeUpdatedFrom::kURLLoaderStatusMonitor);
  }
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  URLLoaderStatusChangeCallback callback_;
};

URLLoaderStatusMonitor::URLLoaderStatusMonitor(
    URLLoaderStatusChangeCallback callback)
    : callback_(std::move(callback)) {}

void URLLoaderStatusMonitor::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  std::move(callback_).Run(status);
}

// static
std::unique_ptr<ResourceDownloader> ResourceDownloader::BeginDownload(
    base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<DownloadUrlParameters> params,
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
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  auto downloader = std::make_unique<ResourceDownloader>(
      delegate, std::move(request), params->render_process_host_id(),
      params->render_frame_host_routing_id(), serialized_embedder_download_data,
      tab_url, tab_referrer_url, is_new_download, task_runner,
      std::move(url_loader_factory), url_security_policy,
      std::move(wake_lock_provider));

  downloader->Start(std::move(params), is_parallel_request, is_background_mode);
  return downloader;
}

// static
void ResourceDownloader::InterceptNavigationResponse(
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
    bool is_transient) {
  auto downloader = std::make_unique<ResourceDownloader>(
      delegate, std::move(resource_request), render_process_id, render_frame_id,
      serialized_embedder_download_data, tab_url, tab_referrer_url, true,
      task_runner, std::move(url_loader_factory), url_security_policy,
      std::move(wake_lock_provider));
  ResourceDownloader* raw_downloader = downloader.get();
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UrlDownloadHandler::Delegate::OnUrlDownloadHandlerCreated, delegate,
          UrlDownloadHandler::UniqueUrlDownloadHandlerPtr(
              std::move(downloader).release(),
              base::OnTaskRunnerDeleter(
                  base::SingleThreadTaskRunner::GetCurrentDefault()))));
  raw_downloader->InterceptResponse(
      std::move(url_chain), cert_status, std::move(response_head),
      std::move(response_body), std::move(url_loader_client_endpoints),
      is_transient);
}

ResourceDownloader::ResourceDownloader(
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
    mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider)
    : delegate_(delegate),
      resource_request_(std::move(resource_request)),
      is_new_download_(is_new_download),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      serialized_embedder_download_data_(serialized_embedder_download_data),
      tab_url_(tab_url),
      tab_referrer_url_(tab_referrer_url),
      delegate_task_runner_(task_runner),
      url_loader_factory_(url_loader_factory),
      url_security_policy_(url_security_policy),
      is_content_initiated_(false) {
  if (wake_lock_provider) {
    mojo::Remote<device::mojom::WakeLockProvider> provider(
        std::move(wake_lock_provider));
    RequestWakeLock(provider.get());
  }
}

ResourceDownloader::~ResourceDownloader() = default;

void ResourceDownloader::Start(
    std::unique_ptr<DownloadUrlParameters> download_url_parameters,
    bool is_parallel_request,
    bool is_background_mode) {
  callback_ = std::move(download_url_parameters->callback());
  upload_callback_ = download_url_parameters->upload_callback();
  guid_ = download_url_parameters->guid();
  is_content_initiated_ = download_url_parameters->content_initiated();

  // Set up the URLLoaderClient.
  url_loader_client_ = std::make_unique<DownloadResponseHandler>(
      resource_request_.get(), this,
      std::make_unique<DownloadSaveInfo>(
          download_url_parameters->TakeSaveInfo()),
      is_parallel_request, download_url_parameters->is_transient(),
      download_url_parameters->fetch_error_body(),
      download_url_parameters->cross_origin_redirects(),
      download_url_parameters->request_headers(),
      download_url_parameters->request_origin(),
      download_url_parameters->download_source(),
      download_url_parameters->require_safety_checks(),
      std::vector<GURL>(1, resource_request_->url), is_background_mode);

  mojo::PendingRemote<network::mojom::URLLoaderClient> url_loader_client_remote;
  url_loader_client_receiver_ =
      std::make_unique<mojo::Receiver<network::mojom::URLLoaderClient>>(
          url_loader_client_.get(),
          url_loader_client_remote.InitWithNewPipeAndPassReceiver());

  // Set up the URLLoader
  url_loader_factory_->CreateLoaderAndStart(
      url_loader_.BindNewPipeAndPassReceiver(),
      0,  // request_id
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
          network::mojom::kURLLoadOptionSniffMimeType,
      *(resource_request_.get()), std::move(url_loader_client_remote),
      net::MutableNetworkTrafficAnnotationTag(
          download_url_parameters->GetNetworkTrafficAnnotation()));
  url_loader_->SetPriority(net::RequestPriority::IDLE,
                           0 /* intra_priority_value */);
}

void ResourceDownloader::InterceptResponse(
    std::vector<GURL> url_chain,
    net::CertStatus cert_status,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    bool is_transient) {
  // Set the URLLoader.
  url_loader_.Bind(std::move(endpoints->url_loader));

#if BUILDFLAG(IS_ANDROID)
  is_must_download_ = IsContentDispositionAttachmentInHead(*response_head);
#endif  // BUILDFLAG(IS_ANDROID)

  // Create the new URLLoaderClient that will intercept the navigation.
  url_loader_client_ = std::make_unique<DownloadResponseHandler>(
      resource_request_.get(), this, std::make_unique<DownloadSaveInfo>(),
      false,               /* is_parallel_request */
      is_transient, false, /* fetch_error_body */
      network::mojom::RedirectMode::kFollow,
      download::DownloadUrlParameters::RequestHeadersType(),
      std::string(),                              /* request_origin */
      download::DownloadSource::NAVIGATION, true, /* require_safety_checks */
      std::move(url_chain), false /* is_background_mode */);

  // Simulate on the new URLLoaderClient calls that happened on the old client.
  response_head->cert_status = cert_status;
  url_loader_client_->OnReceiveResponse(std::move(response_head),
                                        std::move(response_body), std::nullopt);

  // Bind the new client.
  url_loader_client_receiver_ =
      std::make_unique<mojo::Receiver<network::mojom::URLLoaderClient>>(
          url_loader_client_.get(), std::move(endpoints->url_loader_client));
}

void ResourceDownloader::OnResponseStarted(
    std::unique_ptr<DownloadCreateInfo> download_create_info,
    mojom::DownloadStreamHandlePtr stream_handle) {
  download_create_info->is_new_download = is_new_download_;
  download_create_info->guid = guid_;
  download_create_info->serialized_embedder_download_data =
      serialized_embedder_download_data_;
  download_create_info->tab_url = tab_url_;
  download_create_info->tab_referrer_url = tab_referrer_url_;
  download_create_info->render_process_id = render_process_id_;
  download_create_info->render_frame_id = render_frame_id_;
  download_create_info->has_user_gesture = resource_request_->has_user_gesture;
  download_create_info->is_content_initiated = is_content_initiated_;
  download_create_info->transition_type =
      ui::PageTransitionFromInt(resource_request_->transition_type);
#if BUILDFLAG(IS_ANDROID)
  download_create_info->is_must_download = is_must_download_;
#endif  // BUILDFLAG(IS_ANDROID)

  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &UrlDownloadHandler::Delegate::OnUrlDownloadStarted, delegate_,
          std::move(download_create_info),
          std::make_unique<StreamHandleInputStream>(std::move(stream_handle)),
          URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr(
              new URLLoaderFactoryProvider(url_loader_factory_),
              base::OnTaskRunnerDeleter(
                  base::SingleThreadTaskRunner::GetCurrentDefault())),
          reinterpret_cast<UrlDownloadHandlerID>(this), std::move(callback_)));
}

void ResourceDownloader::OnReceiveRedirect() {
  url_loader_->FollowRedirect(
      std::vector<std::string>() /* removed_headers */,
      net::HttpRequestHeaders() /* modified_headers */,
      net::HttpRequestHeaders() /* modified_cors_exempt_headers */,
      std::nullopt);
}

void ResourceDownloader::OnResponseCompleted() {
  Destroy();
}

bool ResourceDownloader::CanRequestURL(const GURL& url) {
  return url_security_policy_
             ? url_security_policy_.Run(render_process_id_, url)
             : true;
}

void ResourceDownloader::OnUploadProgress(uint64_t bytes_uploaded) {
  if (!upload_callback_)
    return;

  delegate_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(upload_callback_, bytes_uploaded));
}

void ResourceDownloader::Destroy() {
  if (wake_lock_)
    wake_lock_->CancelWakeLock();
  // TODO(crbug.com/40248618): Use Weak Pointers instead.
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UrlDownloadHandler::Delegate::OnUrlDownloadStopped,
                     delegate_, reinterpret_cast<UrlDownloadHandlerID>(this)));
}

void ResourceDownloader::RequestWakeLock(
    device::mojom::WakeLockProvider* provider) {
  provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventAppSuspension,
      device::mojom::WakeLockReason::kOther, "Download in progress",
      wake_lock_.BindNewPipeAndPassReceiver());

  wake_lock_->RequestWakeLock();
}

}  // namespace download
