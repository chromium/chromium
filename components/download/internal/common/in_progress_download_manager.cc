// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/in_progress_download_manager.h"

#include "base/bind.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
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
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/download_utils.h"
#include "components/download/public/common/input_stream.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/service_manager/public/cpp/connector.h"

#if defined(OS_ANDROID)
#include "components/download/internal/common/android/download_collection_bridge.h"
#include "components/download/public/common/download_path_reservation_tracker.h"
#endif

namespace download {

namespace {

std::unique_ptr<DownloadItemImpl> CreateDownloadItemImpl(
    DownloadItemImplDelegate* delegate,
    const DownloadDBEntry entry,
    std::unique_ptr<DownloadEntry> download_entry) {
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
      in_progress_info->tab_referrer_url, base::nullopt,
      in_progress_info->mime_type, in_progress_info->original_mime_type,
      in_progress_info->start_time, in_progress_info->end_time,
      in_progress_info->etag, in_progress_info->last_modified,
      in_progress_info->received_bytes, in_progress_info->total_bytes,
      in_progress_info->auto_resume_count, in_progress_info->hash,
      in_progress_info->state, in_progress_info->danger_type,
      in_progress_info->interrupt_reason, in_progress_info->paused,
      in_progress_info->metered, false, base::Time(),
      in_progress_info->transient, in_progress_info->received_slices,
      std::move(download_entry));
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
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    const URLSecurityPolicy& url_security_policy,
    bool is_new_download,
    base::WeakPtr<InProgressDownloadManager> download_manager,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    std::unique_ptr<service_manager::Connector> connector,
    bool is_background_mode,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner) {
  DCHECK(GetIOTaskRunner()->BelongsToCurrentThread());
  UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader(
      ResourceDownloader::BeginDownload(
          download_manager, std::move(params), std::move(request),
          network::SharedURLLoaderFactory::Create(
              std::move(url_loader_factory_info)),
          url_security_policy, site_url, tab_url, tab_referrer_url,
          is_new_download, false, std::move(connector), is_background_mode,
          main_task_runner)
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
    net::CertStatus cert_status,
    scoped_refptr<network::ResourceResponse> response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    const URLSecurityPolicy& url_security_policy,
    std::unique_ptr<service_manager::Connector> connector,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner) {
  DCHECK(GetIOTaskRunner()->BelongsToCurrentThread());
  UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader(
      ResourceDownloader::InterceptNavigationResponse(
          download_manager, std::move(resource_request), render_process_id,
          render_frame_id, site_url, tab_url, tab_referrer_url,
          std::move(url_chain), std::move(cert_status),
          std::move(response_head), std::move(response_body),
          std::move(url_loader_client_endpoints),
          network::SharedURLLoaderFactory::Create(
              std::move(url_loader_factory_info)),
          url_security_policy, std::move(connector), main_task_runner)
          .release(),
      base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));

  OnUrlDownloadHandlerCreated(std::move(downloader), download_manager,
                              main_task_runner);
}

#if defined(OS_ANDROID)
void OnDownloadDisplayNamesReturned(
    DownloadCollectionBridge::GetDisplayNamesCallback callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    InProgressDownloadManager::DisplayNames download_names) {
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(download_names)));
}

void OnPathReserved(
    const DownloadItemImplDelegate::DownloadTargetCallback& callback,
    DownloadDangerType danger_type,
    const InProgressDownloadManager::IntermediatePathCallback&
        intermediate_path_cb,
    const base::FilePath& forced_file_path,
    PathValidationResult result,
    const base::FilePath& target_path) {
  base::FilePath intermediate_path;
  if (!target_path.empty() &&
      (result == PathValidationResult::SUCCESS ||
       result == download::PathValidationResult::SAME_AS_SOURCE)) {
    if (!forced_file_path.empty()) {
      DCHECK_EQ(target_path, forced_file_path);
      intermediate_path = target_path;
    } else if (intermediate_path_cb) {
      intermediate_path = intermediate_path_cb.Run(target_path);
    }
  }

  RecordBackgroundTargetDeterminationResult(
      intermediate_path.empty()
          ? BackgroudTargetDeterminationResultTypes::kPathReservationFailed
          : BackgroudTargetDeterminationResultTypes::kSuccess);
  callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               danger_type, intermediate_path,
               intermediate_path.empty() ? DOWNLOAD_INTERRUPT_REASON_FILE_FAILED
                                         : DOWNLOAD_INTERRUPT_REASON_NONE);
}
#endif

}  // namespace

bool InProgressDownloadManager::Delegate::InterceptDownload(
    const DownloadCreateInfo& download_create_info) {
  return false;
}

base::FilePath
InProgressDownloadManager::Delegate::GetDefaultDownloadDirectory() {
  return base::FilePath();
}

InProgressDownloadManager::InProgressDownloadManager(
    Delegate* delegate,
    const base::FilePath& in_progress_db_dir,
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const IsOriginSecureCallback& is_origin_secure_cb,
    const URLSecurityPolicy& url_security_policy,
    service_manager::Connector* connector)
    : delegate_(delegate),
      file_factory_(new DownloadFileFactory()),
      download_start_observer_(nullptr),
      is_origin_secure_cb_(is_origin_secure_cb),
      url_security_policy_(url_security_policy),
      connector_(connector) {
  Initialize(in_progress_db_dir, db_provider);
}

InProgressDownloadManager::~InProgressDownloadManager() = default;

void InProgressDownloadManager::OnUrlDownloadStarted(
    std::unique_ptr<DownloadCreateInfo> download_create_info,
    std::unique_ptr<InputStream> input_stream,
    URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
        url_loader_factory_provider,
    UrlDownloadHandler* downloader,
    DownloadUrlParameters::OnStartedCallback callback) {
  StartDownload(std::move(download_create_info), std::move(input_stream),
                std::move(url_loader_factory_provider),
                base::BindOnce(&InProgressDownloadManager::CancelUrlDownload,
                               weak_factory_.GetWeakPtr(), downloader),
                std::move(callback));
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

void InProgressDownloadManager::DownloadUrl(
    std::unique_ptr<DownloadUrlParameters> params) {
  if (!CanDownload(params.get()))
    return;

  // Start the new download, the download should be saved to the file path
  // specifcied in the |params|.
  BeginDownload(std::move(params), url_loader_factory_->Clone(),
                true /* is_new_download */, GURL() /* site_url */,
                GURL() /* tab_url */, GURL() /* tab_referral_url */);
}

bool InProgressDownloadManager::CanDownload(DownloadUrlParameters* params) {
  if (!params->is_transient())
    return false;

  if (!url_loader_factory_)
    return false;

  if (params->require_safety_checks())
    return false;

  if (params->file_path().empty())
    return false;

  return true;
}

void InProgressDownloadManager::GetAllDownloads(
    SimpleDownloadManager::DownloadVector* downloads) {
  for (auto& item : in_progress_downloads_)
    downloads->push_back(item.get());
}

DownloadItem* InProgressDownloadManager::GetDownloadByGuid(
    const std::string& guid) {
  for (auto& item : in_progress_downloads_) {
    if (item->GetGuid() == guid)
      return item.get();
  }
  return nullptr;
}

void InProgressDownloadManager::BeginDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    bool is_new_download,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url) {
  std::unique_ptr<network::ResourceRequest> request =
      CreateResourceRequest(params.get());
  GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BeginResourceDownload, std::move(params), std::move(request),
          std::move(url_loader_factory_info), url_security_policy_,
          is_new_download, weak_factory_.GetWeakPtr(), site_url, tab_url,
          tab_referrer_url, connector_ ? connector_->Clone() : nullptr,
          !delegate_ /* is_background_mode */,
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
    net::CertStatus cert_status,
    scoped_refptr<network::ResourceResponse> response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info) {
  GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CreateDownloadHandlerForNavigation, weak_factory_.GetWeakPtr(),
          std::move(resource_request), render_process_id, render_frame_id,
          site_url, tab_url, tab_referrer_url, std::move(url_chain),
          std::move(cert_status), std::move(response_head),
          std::move(response_body), std::move(url_loader_client_endpoints),
          std::move(url_loader_factory_info), url_security_policy_,
          connector_ ? connector_->Clone() : nullptr,
          base::ThreadTaskRunnerHandle::Get()));
}

void InProgressDownloadManager::Initialize(
    const base::FilePath& in_progress_db_dir,
    leveldb_proto::ProtoDatabaseProvider* db_provider) {
  std::unique_ptr<DownloadDB> download_db;

  if (in_progress_db_dir.empty()) {
    download_db = std::make_unique<DownloadDB>();
  } else {
    download_db = std::make_unique<DownloadDBImpl>(
        DownloadNamespace::NAMESPACE_BROWSER_DOWNLOAD, in_progress_db_dir,
        db_provider);
  }

  download_db_cache_ =
      std::make_unique<DownloadDBCache>(std::move(download_db));
  if (GetDownloadDBTaskRunnerForTesting()) {
    download_db_cache_->SetTimerTaskRunnerForTesting(
        GetDownloadDBTaskRunnerForTesting());
  }
  download_db_cache_->Initialize(base::BindOnce(
      &InProgressDownloadManager::OnDBInitialized, weak_factory_.GetWeakPtr()));
}

void InProgressDownloadManager::ShutDown() {
  url_download_handlers_.clear();
}

void InProgressDownloadManager::DetermineDownloadTarget(
    DownloadItemImpl* download,
    const DownloadTargetCallback& callback) {
#if defined(OS_ANDROID)
  base::FilePath target_path = download->GetForcedFilePath().empty()
                                   ? download->GetTargetFilePath()
                                   : download->GetForcedFilePath();

  if (target_path.empty()) {
    callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 download->GetDangerType(), target_path,
                 DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
    RecordBackgroundTargetDeterminationResult(
        BackgroudTargetDeterminationResultTypes::kTargetPathMissing);
    return;
  }

  // If final target is a content URI, the intermediate path should
  // be identical to it.
  if (target_path.IsContentUri()) {
    callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 download->GetDangerType(), target_path,
                 DOWNLOAD_INTERRUPT_REASON_NONE);
    RecordBackgroundTargetDeterminationResult(
        BackgroudTargetDeterminationResultTypes::kSuccess);
    return;
  }

  DownloadPathReservationTracker::GetReservedPath(
      download, target_path, target_path.DirName(), default_download_dir_,
      true /* create_directory */,
      download->GetForcedFilePath().empty()
          ? DownloadPathReservationTracker::UNIQUIFY
          : DownloadPathReservationTracker::OVERWRITE,
      base::Bind(&OnPathReserved, callback, download->GetDangerType(),
                 intermediate_path_cb_, download->GetForcedFilePath()));
#else
  callback.Run(download->GetTargetFilePath(),
               DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               download->GetDangerType(), download->GetFullPath(),
               DOWNLOAD_INTERRUPT_REASON_NONE);
#endif  // defined(OS_ANDROID)
}

void InProgressDownloadManager::ResumeInterruptedDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    const GURL& site_url) {
  if (!url_loader_factory_)
    return;

  BeginDownload(std::move(params), url_loader_factory_->Clone(), false,
                site_url, GURL(), GURL());
}

bool InProgressDownloadManager::ShouldOpenDownload(
    DownloadItemImpl* item,
    const ShouldOpenDownloadCallback& callback) {
  return true;
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
    URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
        url_loader_factory_provider,
    DownloadJob::CancelRequestCallback cancel_request_callback,
    DownloadUrlParameters::OnStartedCallback on_started) {
  DCHECK(info);

  if (info->is_new_download &&
      (info->result == DOWNLOAD_INTERRUPT_REASON_NONE ||
       info->result ==
           DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT)) {
    if (delegate_ && delegate_->InterceptDownload(*info)) {
      if (cancel_request_callback)
        std::move(cancel_request_callback).Run(false);
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
    RecordDownloadContentTypeSecurity(info->url(), info->url_chain,
                                      info->mime_type, is_origin_secure_cb_);
  }

  // If the download cannot be found locally, ask |delegate_| to provide the
  // DownloadItem.
  if (delegate_ && !GetDownloadByGuid(info->guid)) {
    delegate_->StartDownloadItem(
        std::move(info), std::move(on_started),
        base::BindOnce(&InProgressDownloadManager::StartDownloadWithItem,
                       weak_factory_.GetWeakPtr(), std::move(stream),
                       std::move(url_loader_factory_provider),
                       std::move(cancel_request_callback)));
  } else {
    std::string guid = info->guid;
    if (info->is_new_download) {
      auto download = std::make_unique<DownloadItemImpl>(
          this, DownloadItem::kInvalidId, *info);
      OnNewDownloadCreated(download.get());
      in_progress_downloads_.push_back(std::move(download));
    }
    StartDownloadWithItem(
        std::move(stream), std::move(url_loader_factory_provider),
        std::move(cancel_request_callback), std::move(info),
        static_cast<DownloadItemImpl*>(GetDownloadByGuid(guid)), false);
  }
}

void InProgressDownloadManager::StartDownloadWithItem(
    std::unique_ptr<InputStream> stream,
    URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
        url_loader_factory_provider,
    DownloadJob::CancelRequestCallback cancel_request_callback,
    std::unique_ptr<DownloadCreateInfo> info,
    DownloadItemImpl* download,
    bool should_persist_new_download) {
  if (!download) {
    // If the download is no longer known to the DownloadManager, then it was
    // removed after it was resumed. Ignore. If the download is cancelled
    // while resuming, then also ignore the request.
    if (cancel_request_callback)
      std::move(cancel_request_callback).Run(false);
    // The ByteStreamReader lives and dies on the download sequence.
    if (info->result == DOWNLOAD_INTERRUPT_REASON_NONE)
      GetDownloadTaskRunner()->DeleteSoon(FROM_HERE, stream.release());
    return;
  }

  base::FilePath default_download_directory;
  if (delegate_)
    default_download_directory = delegate_->GetDefaultDownloadDirectory();

  if (info->is_new_download && !should_persist_new_download)
    non_persistent_download_guids_.insert(download->GetGuid());
  // If the download is not persisted, don't notify |download_db_cache_|.
  if (!base::Contains(non_persistent_download_guids_, download->GetGuid())) {
    download_db_cache_->AddOrReplaceEntry(
        CreateDownloadDBEntryFromItem(*download));
    download->RemoveObserver(download_db_cache_.get());
    download->AddObserver(download_db_cache_.get());
  }

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

  download->Start(std::move(download_file), std::move(cancel_request_callback),
                  *info, std::move(url_loader_factory_provider));

  if (download_start_observer_)
    download_start_observer_->OnDownloadStarted(download);
}

void InProgressDownloadManager::OnDBInitialized(
    bool success,
    std::unique_ptr<std::vector<DownloadDBEntry>> entries) {
#if defined(OS_ANDROID)
  if (entries->size() > 0 &&
      DownloadCollectionBridge::NeedToRetrieveDisplayNames()) {
    DownloadCollectionBridge::GetDisplayNamesCallback callback =
        base::BindOnce(&InProgressDownloadManager::OnDownloadNamesRetrieved,
                       weak_factory_.GetWeakPtr(), std::move(entries));
    GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DownloadCollectionBridge::GetDisplayNamesForDownloads,
            base::BindOnce(&OnDownloadDisplayNamesReturned, std::move(callback),
                           base::ThreadTaskRunnerHandle::Get())));
    return;
  }
#endif
  OnDownloadNamesRetrieved(std::move(entries), nullptr);
}

void InProgressDownloadManager::OnDownloadNamesRetrieved(
    std::unique_ptr<std::vector<DownloadDBEntry>> entries,
    DisplayNames display_names) {
  std::set<uint32_t> download_ids;
  int num_duplicates = 0;
  display_names_ = std::move(display_names);
  for (const auto& entry : *entries) {
    auto item = CreateDownloadItemImpl(
        this, entry, CreateDownloadEntryFromDownloadDBEntry(entry));
    if (!item)
      continue;
    uint32_t download_id = item->GetId();
    // Remove entries with duplicate ids.
    if (download_id != DownloadItem::kInvalidId &&
        base::Contains(download_ids, download_id)) {
      RemoveInProgressDownload(item->GetGuid());
      num_duplicates++;
      continue;
    }
#if defined(OS_ANDROID)
    const base::FilePath& path = item->GetTargetFilePath();
    if (path.IsContentUri()) {
      base::FilePath display_name = GetDownloadDisplayName(path);
      // If a download doesn't have a display name, remove it.
      if (display_name.empty()) {
        RemoveInProgressDownload(item->GetGuid());
        continue;
      } else {
        item->SetDisplayName(display_name);
      }
    }
#endif
    item->AddObserver(download_db_cache_.get());
    OnNewDownloadCreated(item.get());
    in_progress_downloads_.emplace_back(std::move(item));
    download_ids.insert(download_id);
  }
  if (num_duplicates > 0)
    RecordDuplicateInProgressDownloadIdCount(num_duplicates);
  OnInitialized();
  OnDownloadsInitialized();
}

std::vector<std::unique_ptr<download::DownloadItemImpl>>
InProgressDownloadManager::TakeInProgressDownloads() {
  return std::move(in_progress_downloads_);
}

base::FilePath InProgressDownloadManager::GetDownloadDisplayName(
    const base::FilePath& path) {
#if defined(OS_ANDROID)
  if (!display_names_)
    return base::FilePath();
  auto iter = display_names_->find(path.value());
  if (iter != display_names_->end())
    return iter->second;
#endif
  return base::FilePath();
}

void InProgressDownloadManager::OnAllInprogressDownloadsLoaded() {
  display_names_.reset();
}

void InProgressDownloadManager::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (initialized_)
    OnDownloadsInitialized();
}

void InProgressDownloadManager::OnDownloadsInitialized() {
  if (delegate_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&InProgressDownloadManager::NotifyDownloadsInitialized,
                       weak_factory_.GetWeakPtr()));
  }
}

void InProgressDownloadManager::NotifyDownloadsInitialized() {
  if (delegate_)
    delegate_->OnDownloadsInitialized();
}

void InProgressDownloadManager::AddInProgressDownloadForTest(
    std::unique_ptr<download::DownloadItemImpl> download) {
  in_progress_downloads_.push_back(std::move(download));
}

void InProgressDownloadManager::CancelUrlDownload(
    UrlDownloadHandler* downloader,
    bool user_cancel) {
  OnUrlDownloadStopped(downloader);
}

}  // namespace download
