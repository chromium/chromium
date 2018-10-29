// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/in_progress_download_manager.h"

#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/database/download_db_entry.h"
#include "components/download/database/download_db_impl.h"
#include "components/download/database/download_namespace.h"
#include "components/download/internal/common/download_db_cache.h"
#include "components/download/internal/common/resource_downloader.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_file.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/download/public/common/download_start_observer.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/download_url_loader_factory_getter.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/download_utils.h"
#include "components/download/public/common/input_stream.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"

namespace download {

namespace {

std::unique_ptr<DownloadItemImpl> CreateDownloadItemImpl(
    DownloadItemImplDelegate* delegate,
    const DownloadDBEntry entry) {
  if (!entry.download_info)
    return nullptr;

  // DownloadDBEntry migrated from in-progress cache has negative Ids.
  if (entry.download_info->id < 0)
    return nullptr;

  base::Optional<InProgressInfo> in_progress_info =
      entry.download_info->in_progress_info;
  if (!in_progress_info)
    return nullptr;
  return std::make_unique<DownloadItemImpl>(
      delegate, entry.download_info->guid, entry.download_info->id,
      in_progress_info->current_path, in_progress_info->target_path,
      in_progress_info->url_chain, in_progress_info->referrer_url,
      in_progress_info->site_url, in_progress_info->tab_url,
      in_progress_info->tab_referrer_url, in_progress_info->mime_type,
      in_progress_info->original_mime_type, in_progress_info->start_time,
      in_progress_info->end_time, in_progress_info->etag,
      in_progress_info->last_modified, in_progress_info->received_bytes,
      in_progress_info->total_bytes, in_progress_info->hash,
      in_progress_info->state, in_progress_info->danger_type,
      in_progress_info->interrupt_reason, false, base::Time(),
      in_progress_info->transient, in_progress_info->received_slices);
}

void OnUrlDownloadHandlerCreated(
    UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader,
    base::WeakPtr<InProgressDownloadManager> download_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner) {
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&UrlDownloadHandler::Delegate::OnUrlDownloadHandlerCreated,
                     download_manager, std::move(downloader)));
}

void BeginResourceDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
    bool is_new_download,
    base::WeakPtr<InProgressDownloadManager> download_manager,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner) {
  DCHECK(GetIOTaskRunner()->BelongsToCurrentThread());
  UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader(
      ResourceDownloader::BeginDownload(
          download_manager, std::move(params), std::move(request),
          std::move(url_loader_factory_getter), site_url, tab_url,
          tab_referrer_url, is_new_download, false, main_task_runner)
          .release(),
      base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));

  OnUrlDownloadHandlerCreated(std::move(downloader), download_manager,
                              main_task_runner);
}

void CreateDownloadHandlerForNavigation(
    base::WeakPtr<InProgressDownloadManager> download_manager,
    std::unique_ptr<network::ResourceRequest> resource_request,
    int render_process_id,
    int render_frame_id,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    std::vector<GURL> url_chain,
    scoped_refptr<network::ResourceResponse> response,
    net::CertStatus cert_status,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner) {
  DCHECK(GetIOTaskRunner()->BelongsToCurrentThread());
  UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader(
      ResourceDownloader::InterceptNavigationResponse(
          download_manager, std::move(resource_request), render_process_id,
          render_frame_id, site_url, tab_url, tab_referrer_url,
          std::move(url_chain), std::move(response), std::move(cert_status),
          std::move(url_loader_client_endpoints),
          std::move(url_loader_factory_getter), main_task_runner)
          .release(),
      base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));

  OnUrlDownloadHandlerCreated(std::move(downloader), download_manager,
                              main_task_runner);
}

}  // namespace

InProgressDownloadManager::InProgressDownloadManager(
    Delegate* delegate,
    const base::FilePath& in_progress_db_dir,
    const IsOriginSecureCallback& is_origin_secure_cb)
    : is_initialized_(false),
      delegate_(delegate),
      file_factory_(new DownloadFileFactory()),
      download_start_observer_(nullptr),
      is_origin_secure_cb_(is_origin_secure_cb),
      weak_factory_(this) {
  Initialize(in_progress_db_dir);
}

InProgressDownloadManager::~InProgressDownloadManager() = default;

void InProgressDownloadManager::OnUrlDownloadStarted(
    std::unique_ptr<DownloadCreateInfo> download_create_info,
    std::unique_ptr<InputStream> input_stream,
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
    const DownloadUrlParameters::OnStartedCallback& callback) {
  StartDownload(std::move(download_create_info), std::move(input_stream),
                std::move(url_loader_factory_getter), callback);
}

void InProgressDownloadManager::OnUrlDownloadStopped(
    UrlDownloadHandler* downloader) {
  for (auto ptr = url_download_handlers_.begin();
       ptr != url_download_handlers_.end(); ++ptr) {
    if (ptr->get() == downloader) {
      url_download_handlers_.erase(ptr);
      return;
    }
  }
}

void InProgressDownloadManager::OnUrlDownloadHandlerCreated(
    UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader) {
  if (downloader)
    url_download_handlers_.push_back(std::move(downloader));
}

void InProgressDownloadManager::BeginDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
    bool is_new_download,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url) {
  std::unique_ptr<network::ResourceRequest> request =
      CreateResourceRequest(params.get());
  GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BeginResourceDownload, std::move(params),
                     std::move(request), std::move(url_loader_factory_getter),
                     is_new_download, weak_factory_.GetWeakPtr(), site_url,
                     tab_url, tab_referrer_url,
                     base::ThreadTaskRunnerHandle::Get()));
}

void InProgressDownloadManager::InterceptDownloadFromNavigation(
    std::unique_ptr<network::ResourceRequest> resource_request,
    int render_process_id,
    int render_frame_id,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    std::vector<GURL> url_chain,
    scoped_refptr<network::ResourceResponse> response,
    net::CertStatus cert_status,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter) {
  GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateDownloadHandlerForNavigation,
                     weak_factory_.GetWeakPtr(), std::move(resource_request),
                     render_process_id, render_frame_id, site_url, tab_url,
                     tab_referrer_url, std::move(url_chain),
                     std::move(response), std::move(cert_status),
                     std::move(url_loader_client_endpoints),
                     std::move(url_loader_factory_getter),
                     base::ThreadTaskRunnerHandle::Get()));
}

void InProgressDownloadManager::Initialize(
    const base::FilePath& in_progress_db_dir) {
  download_db_cache_ = std::make_unique<DownloadDBCache>(
      in_progress_db_dir.empty()
          ? std::make_unique<DownloadDB>()
          : std::make_unique<DownloadDBImpl>(
                DownloadNamespace::NAMESPACE_BROWSER_DOWNLOAD,
                in_progress_db_dir));
  download_db_cache_->Initialize(base::BindOnce(
      &InProgressDownloadManager::OnInitialized, weak_factory_.GetWeakPtr()));
}

void InProgressDownloadManager::ShutDown() {
  url_download_handlers_.clear();
}

void InProgressDownloadManager::DetermineDownloadTarget(
    DownloadItemImpl* download,
    const DownloadTargetCallback& callback) {
  // TODO(http://crbug.com/851581): handle the case that |target_path| and
  // |intermediate_path| are empty.
  base::FilePath target_path = download->GetTargetFilePath().empty()
                                   ? download->GetForcedFilePath()
                                   : download->GetTargetFilePath();
  base::FilePath intermediate_path = download->GetFullPath().empty()
                                         ? download->GetForcedFilePath()
                                         : download->GetFullPath();
  callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               download->GetDangerType(), intermediate_path,
               DOWNLOAD_INTERRUPT_REASON_NONE);
}

void InProgressDownloadManager::ResumeInterruptedDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    const GURL& site_url) {
  if (!url_loader_factory_getter_)
    return;

  BeginDownload(std::move(params), url_loader_factory_getter_, false, site_url,
                GURL(), GURL());
}

bool InProgressDownloadManager::ShouldOpenDownload(
    DownloadItemImpl* item,
    const ShouldOpenDownloadCallback& callback) {
  return true;
}

base::Optional<DownloadEntry> InProgressDownloadManager::GetInProgressEntry(
    DownloadItemImpl* download) {
  if (!download)
    return base::Optional<DownloadEntry>();

  return CreateDownloadEntryFromDownloadDBEntry(
      download_db_cache_->RetrieveEntry(download->GetGuid()));
  return base::Optional<DownloadEntry>();
}

void InProgressDownloadManager::ReportBytesWasted(DownloadItemImpl* download) {
  download_db_cache_->OnDownloadUpdated(download);
}

void InProgressDownloadManager::RemoveInProgressDownload(
    const std::string& guid) {
  download_db_cache_->RemoveEntry(guid);
}

void InProgressDownloadManager::StartDownload(
    std::unique_ptr<DownloadCreateInfo> info,
    std::unique_ptr<InputStream> stream,
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
    const DownloadUrlParameters::OnStartedCallback& on_started) {
  DCHECK(info);

  if (info->is_new_download &&
      (info->result == DOWNLOAD_INTERRUPT_REASON_NONE ||
       info->result ==
           DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT)) {
    if (delegate_ && delegate_->InterceptDownload(*info)) {
      GetDownloadTaskRunner()->DeleteSoon(FROM_HERE, stream.release());
      return;
    }
  }

  // |stream| is only non-null if the download request was successful.
  DCHECK(
      (info->result == DOWNLOAD_INTERRUPT_REASON_NONE && !stream->IsEmpty()) ||
      (info->result != DOWNLOAD_INTERRUPT_REASON_NONE && stream->IsEmpty()));
  DVLOG(20) << __func__
            << "() result=" << DownloadInterruptReasonToString(info->result);

  GURL url = info->url();
  std::vector<GURL> url_chain = info->url_chain;
  std::string mime_type = info->mime_type;

  if (info->is_new_download) {
    RecordDownloadConnectionSecurity(info->url(), info->url_chain);
    RecordDownloadContentTypeSecurity(info->url(), info->url_chain,
                                      info->mime_type, is_origin_secure_cb_);
  }

  if (delegate_) {
    delegate_->StartDownloadItem(
        std::move(info), on_started,
        base::BindOnce(&InProgressDownloadManager::StartDownloadWithItem,
                       weak_factory_.GetWeakPtr(), std::move(stream),
                       std::move(url_loader_factory_getter)));
  } else {
    std::string guid = info->guid;
    StartDownloadWithItem(std::move(stream),
                          std::move(url_loader_factory_getter), std::move(info),
                          GetInProgressDownload(guid));
  }
}

void InProgressDownloadManager::StartDownloadWithItem(
    std::unique_ptr<InputStream> stream,
    scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
    std::unique_ptr<DownloadCreateInfo> info,
    DownloadItemImpl* download) {
  if (!download) {
    // If the download is no longer known to the DownloadManager, then it was
    // removed after it was resumed. Ignore. If the download is cancelled
    // while resuming, then also ignore the request.
    if (info->request_handle)
      info->request_handle->CancelRequest(true);
    // The ByteStreamReader lives and dies on the download sequence.
    if (info->result == DOWNLOAD_INTERRUPT_REASON_NONE)
      GetDownloadTaskRunner()->DeleteSoon(FROM_HERE, stream.release());
    return;
  }

  base::FilePath default_download_directory;
  if (delegate_)
    default_download_directory = delegate_->GetDefaultDownloadDirectory();

  base::Optional<DownloadDBEntry> entry_opt =
      download_db_cache_->RetrieveEntry(download->GetGuid());
  if (!entry_opt.has_value()) {
    download_db_cache_->AddOrReplaceEntry(CreateDownloadDBEntryFromItem(
        *download, UkmInfo(info->download_source, GetUniqueDownloadId()),
        info->fetch_error_body, info->request_headers));
    }
    download->RemoveObserver(download_db_cache_.get());
    download->AddObserver(download_db_cache_.get());

  std::unique_ptr<DownloadFile> download_file;
  if (info->result == DOWNLOAD_INTERRUPT_REASON_NONE) {
    DCHECK(stream);
    download_file.reset(file_factory_->CreateFile(
        std::move(info->save_info), default_download_directory,
        std::move(stream), download->GetId(),
        download->DestinationObserverAsWeakPtr()));
  }
  // It is important to leave info->save_info intact in the case of an interrupt
  // so that the DownloadItem can salvage what it can out of a failed
  // resumption attempt.

  download->Start(
      std::move(download_file), std::move(info->request_handle), *info,
      std::move(url_loader_factory_getter),
      delegate_ ? delegate_->GetURLRequestContextGetter(*info) : nullptr);

  if (download_start_observer_)
    download_start_observer_->OnDownloadStarted(download);
}

void InProgressDownloadManager::OnInitialized(
    bool success,
    std::unique_ptr<std::vector<DownloadDBEntry>> entries) {
  // Destroy the in-progress cache as it is no longer needed on success.
  if (base::FeatureList::IsEnabled(features::kDownloadDBForNewDownloads)) {
    for (const auto& entry : *entries) {
      auto item = CreateDownloadItemImpl(this, entry);
      if (!item)
        continue;
      item->AddObserver(download_db_cache_.get());
      in_progress_downloads_.emplace_back(std::move(item));
    }
  }
  is_initialized_ = true;
  for (auto& callback : on_initialized_callbacks_)
    std::move(*callback).Run();
  on_initialized_callbacks_.clear();
}

DownloadItemImpl* InProgressDownloadManager::GetInProgressDownload(
    const std::string& guid) {
  for (auto& item : in_progress_downloads_) {
    if (item->GetGuid() == guid)
      return item.get();
  }
  return nullptr;
}

void InProgressDownloadManager::NotifyWhenInitialized(
    base::OnceClosure on_initialized_cb) {
  if (is_initialized_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(on_initialized_cb));
    return;
  }
  on_initialized_callbacks_.emplace_back(
      std::make_unique<base::OnceClosure>(std::move(on_initialized_cb)));
}

std::vector<std::unique_ptr<download::DownloadItemImpl>>
InProgressDownloadManager::TakeInProgressDownloads() {
  return std::move(in_progress_downloads_);
}

}  // namespace download
