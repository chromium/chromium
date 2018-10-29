// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/internal/download_driver_impl.h"

#include <set>
#include <vector>

#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/download/internal/background_service/driver_entry.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_url_parameters.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"

namespace download {

namespace {

// Converts a DownloadItem::DownloadState to DriverEntry::State.
DriverEntry::State ToDriverEntryState(
    DownloadItem::DownloadState state) {
  switch (state) {
    case DownloadItem::IN_PROGRESS:
      return DriverEntry::State::IN_PROGRESS;
    case DownloadItem::COMPLETE:
      return DriverEntry::State::COMPLETE;
    case DownloadItem::CANCELLED:
      return DriverEntry::State::CANCELLED;
    case DownloadItem::INTERRUPTED:
      return DriverEntry::State::INTERRUPTED;
    case DownloadItem::MAX_DOWNLOAD_STATE:
      return DriverEntry::State::UNKNOWN;
    default:
      NOTREACHED();
      return DriverEntry::State::UNKNOWN;
  }
}

FailureType FailureTypeFromInterruptReason(DownloadInterruptReason reason) {
  switch (reason) {
    case DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE:
    case DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG:
    case DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE:
    case DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN:
    case DOWNLOAD_INTERRUPT_REASON_USER_CANCELED:
      return FailureType::NOT_RECOVERABLE;
    default:
      return FailureType::RECOVERABLE;
  }
}

// Logs interrupt reason when download fails.
void LogDownloadInterruptReason(download::DownloadInterruptReason reason) {
  base::UmaHistogramSparse("Download.Service.Driver.InterruptReason", reason);
}

}  // namespace

// static
DriverEntry DownloadDriverImpl::CreateDriverEntry(
    const DownloadItem* item) {
  DCHECK(item);
  DriverEntry entry;
  entry.guid = item->GetGuid();
  entry.state = ToDriverEntryState(item->GetState());
  entry.paused = item->IsPaused();
  entry.done = item->IsDone();
  entry.bytes_downloaded = item->GetReceivedBytes();
  entry.expected_total_size = item->GetTotalBytes();
  entry.current_file_path =
      item->GetState() == DownloadItem::DownloadState::COMPLETE
          ? item->GetTargetFilePath()
          : item->GetFullPath();
  entry.completion_time = item->GetEndTime();
  entry.response_headers = item->GetResponseHeaders();
  if (entry.response_headers) {
    entry.can_resume =
        entry.response_headers->HasHeaderValue("Accept-Ranges", "bytes") ||
        (entry.response_headers->HasHeader("Content-Range") &&
         entry.response_headers->response_code() == net::HTTP_PARTIAL_CONTENT);
    entry.can_resume &= entry.response_headers->HasStrongValidators();
  } else {
    // If we haven't issued the request yet, treat this like a resume based on
    // the etag and last modified time.
    entry.can_resume =
        !item->GetETag().empty() || !item->GetLastModifiedTime().empty();
  }
  entry.url_chain = item->GetUrlChain();
  return entry;
}

DownloadDriverImpl::DownloadDriverImpl(content::DownloadManager* manager)
    : download_manager_(manager), client_(nullptr), weak_ptr_factory_(this) {
  DCHECK(download_manager_);
}

DownloadDriverImpl::~DownloadDriverImpl() = default;

void DownloadDriverImpl::Initialize(DownloadDriver::Client* client) {
  DCHECK(!client_);
  client_ = client;
  DCHECK(client_);

  // |download_manager_| may be shut down. Informs the client.
  if (!download_manager_) {
    client_->OnDriverReady(false);
    return;
  }

  notifier_ =
      std::make_unique<AllDownloadItemNotifier>(download_manager_, this);
}

void DownloadDriverImpl::HardRecover() {
  // TODO(dtrainor, xingliu): Implement recovery for the DownloadManager.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DownloadDriverImpl::OnHardRecoverComplete,
                                weak_ptr_factory_.GetWeakPtr(), true));
}

bool DownloadDriverImpl::IsReady() const {
  return client_ && download_manager_ &&
         download_manager_->IsManagerInitialized();
}

void DownloadDriverImpl::Start(
    const RequestParams& request_params,
    const std::string& guid,
    const base::FilePath& file_path,
    scoped_refptr<network::ResourceRequestBody> post_body,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(!request_params.url.is_empty());
  DCHECK(!guid.empty());
  if (!download_manager_)
    return;

  std::unique_ptr<DownloadUrlParameters> download_url_params(
      new DownloadUrlParameters(request_params.url,
                                traffic_annotation));

  // TODO(xingliu): Make content::DownloadManager handle potential guid
  // collision and return an error to fail the download cleanly.
  for (net::HttpRequestHeaders::Iterator it(request_params.request_headers);
       it.GetNext();) {
    download_url_params->add_request_header(it.name(), it.value());
  }
  download_url_params->set_guid(guid);
  download_url_params->set_transient(true);
  download_url_params->set_method(request_params.method);
  download_url_params->set_file_path(file_path);
  if (request_params.fetch_error_body)
    download_url_params->set_fetch_error_body(true);
  download_url_params->set_download_source(
      download::DownloadSource::INTERNAL_API);
  download_url_params->set_post_body(post_body);

  download_manager_->DownloadUrl(std::move(download_url_params),
                                 nullptr /* blob_data_handle */,
                                 nullptr /* blob_url_loader_factory */);
}

void DownloadDriverImpl::Remove(const std::string& guid, bool remove_file) {
  guid_to_remove_.emplace(guid);

  // DownloadItem::Remove will cause the item object removed from memory, post
  // the remove task to avoid the object being accessed in the same call stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadDriverImpl::DoRemoveDownload,
                     weak_ptr_factory_.GetWeakPtr(), guid, remove_file));
}

void DownloadDriverImpl::DoRemoveDownload(const std::string& guid,
                                          bool remove_file) {
  if (!download_manager_)
    return;
  DownloadItem* item = download_manager_->GetDownloadByGuid(guid);
  // Cancels the download and removes the persisted records in content layer.
  if (item) {
    // Remove the download file for completed download.
    if (remove_file)
      item->DeleteFile(base::DoNothing());
    item->Remove();
  }
}

void DownloadDriverImpl::Pause(const std::string& guid) {
  if (!download_manager_)
    return;
  DownloadItem* item = download_manager_->GetDownloadByGuid(guid);
  if (item)
    item->Pause();
}

void DownloadDriverImpl::Resume(const std::string& guid) {
  if (!download_manager_)
    return;
  DownloadItem* item = download_manager_->GetDownloadByGuid(guid);
  if (item)
    item->Resume();
}

base::Optional<DriverEntry> DownloadDriverImpl::Find(const std::string& guid) {
  if (!download_manager_)
    return base::nullopt;
  DownloadItem* item = download_manager_->GetDownloadByGuid(guid);
  if (item)
    return CreateDriverEntry(item);
  return base::nullopt;
}

std::set<std::string> DownloadDriverImpl::GetActiveDownloads() {
  std::set<std::string> guids;
  if (!download_manager_)
    return guids;

  std::vector<DownloadItem*> items;
  download_manager_->GetAllDownloads(&items);

  for (auto* item : items) {
    DriverEntry::State state = ToDriverEntryState(item->GetState());
    if (state == DriverEntry::State::IN_PROGRESS)
      guids.insert(item->GetGuid());
  }

  return guids;
}

size_t DownloadDriverImpl::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(guid_to_remove_) +
         notifier_->EstimateMemoryUsage();
}

void DownloadDriverImpl::OnDownloadUpdated(content::DownloadManager* manager,
                                           DownloadItem* item) {
  DCHECK(client_);
  // Blocks the observer call if we asked to remove the download.
  if (guid_to_remove_.find(item->GetGuid()) != guid_to_remove_.end())
    return;

  using DownloadState = DownloadItem::DownloadState;
  DownloadState state = item->GetState();
  download::DownloadInterruptReason reason = item->GetLastReason();
  DriverEntry entry = CreateDriverEntry(item);

  if (state == DownloadState::COMPLETE) {
    client_->OnDownloadSucceeded(entry);
  } else if (state == DownloadState::IN_PROGRESS) {
    client_->OnDownloadUpdated(entry);
  } else if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    if (client_->IsTrackingDownload(item->GetGuid()))
      LogDownloadInterruptReason(reason);
    client_->OnDownloadFailed(entry, FailureTypeFromInterruptReason(reason));
  }
}

void DownloadDriverImpl::OnDownloadRemoved(content::DownloadManager* manager,
                                           DownloadItem* download) {
  guid_to_remove_.erase(download->GetGuid());
  // |download| is about to be deleted.
}

void DownloadDriverImpl::OnDownloadCreated(content::DownloadManager* manager,
                                           DownloadItem* item) {
  if (guid_to_remove_.find(item->GetGuid()) != guid_to_remove_.end()) {
    // Client has removed the download before content persistence layer created
    // the record, remove the download immediately.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DownloadDriverImpl::DoRemoveDownload,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  item->GetGuid(), false /* remove_file */));
    return;
  }

  // Listens to all downloads.
  DCHECK(client_);
  DriverEntry entry = CreateDriverEntry(item);

  // Only notifies the client about new downloads. Existing download data will
  // be loaded before the driver is ready.
  if (IsReady())
    client_->OnDownloadCreated(entry);
}

void DownloadDriverImpl::OnManagerInitialized(
    content::DownloadManager* manager) {
  DCHECK_EQ(download_manager_, manager);
  DCHECK(client_);
  DCHECK(download_manager_);
  client_->OnDriverReady(true);
}

void DownloadDriverImpl::OnManagerGoingDown(content::DownloadManager* manager) {
  DCHECK_EQ(download_manager_, manager);
  download_manager_ = nullptr;
}

void DownloadDriverImpl::OnHardRecoverComplete(bool success) {
  client_->OnDriverHardRecoverComplete(success);
}

}  // namespace download
