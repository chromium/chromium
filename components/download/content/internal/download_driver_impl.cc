// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/internal/download_driver_impl.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/download/internal/background_service/driver_entry.h"
#include "components/download/network/download_http_utils.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "net/http/http_byte_range.h"
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
      NOTREACHED_IN_MIGRATION();
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

  if (item->GetState() == DownloadItem::DownloadState::COMPLETE) {
    std::string hash = item->GetHash();
    if (!hash.empty()) {
      entry.hash256 = base::HexEncode(hash);
    }
  }

  return entry;
}

DownloadDriverImpl::DownloadDriverImpl(
    SimpleDownloadManagerCoordinator* download_manager_coordinator)
    : client_(nullptr),
      download_manager_coordinator_(download_manager_coordinator),
      is_ready_(false) {
  DCHECK(download_manager_coordinator_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DownloadDriverImpl::~DownloadDriverImpl() {
  if (download_manager_coordinator_)
    download_manager_coordinator_->GetNotifier()->RemoveObserver(this);

  CHECK(!IsInObserverList());
}

void DownloadDriverImpl::Initialize(DownloadDriver::Client* client) {
  DCHECK(!client_);
  client_ = client;
  DCHECK(client_);

  // |download_manager_| may be shut down. Informs the client.
  if (!download_manager_coordinator_) {
    client_->OnDriverReady(false);
    return;
  }

  download_manager_coordinator_->GetNotifier()->AddObserver(this);
}

void DownloadDriverImpl::HardRecover() {
  // TODO(dtrainor, xingliu): Implement recovery for the DownloadManager.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DownloadDriverImpl::OnHardRecoverComplete,
                                weak_ptr_factory_.GetWeakPtr(), true));
}

bool DownloadDriverImpl::IsReady() const {
  return client_ && download_manager_coordinator_ &&
         download_manager_coordinator_->initialized();
}

void DownloadDriverImpl::Start(
    const RequestParams& request_params,
    const std::string& guid,
    const base::FilePath& file_path,
    scoped_refptr<network::ResourceRequestBody> post_body,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(!request_params.url.is_empty());
  DCHECK(!guid.empty());
  if (!download_manager_coordinator_)
    return;

  std::unique_ptr<DownloadUrlParameters> download_url_params(
      new DownloadUrlParameters(request_params.url, traffic_annotation));

  // TODO(xingliu): Make content::DownloadManager handle potential guid
  // collision and return an error to fail the download cleanly.
  for (net::HttpRequestHeaders::Iterator it(request_params.request_headers);
       it.GetNext();) {
    // Range and If-Range are managed by download core instead.
    if (it.name() == net::HttpRequestHeaders::kRange ||
        it.name() == net::HttpRequestHeaders::kIfRange) {
      continue;
    }

    download_url_params->add_request_header(it.name(), it.value());
  }

  if (request_params.request_headers.HasHeader(
          net::HttpRequestHeaders::kRange)) {
    std::optional<net::HttpByteRange> byte_range =
        ParseRangeHeader(request_params.request_headers);
    if (byte_range.has_value()) {
      download_url_params->set_use_if_range(false);
      if (byte_range->IsSuffixByteRange()) {
        download_url_params->set_range_request_offset(
            kInvalidRange, byte_range->suffix_length());
      } else {
        download_url_params->set_range_request_offset(
            byte_range->first_byte_position(),
            byte_range->last_byte_position());
      }
    } else {
      // The request headers are validated in ControllerImpl::StartDownload.
      LOG(ERROR) << "Failed to parse Range request header.";
      NOTREACHED_IN_MIGRATION();
      return;
    }
  }

  download_url_params->set_guid(guid);
  download_url_params->set_transient(true);
  download_url_params->set_method(request_params.method);
  download_url_params->set_credentials_mode(request_params.credentials_mode);
  download_url_params->set_file_path(file_path);
  if (request_params.fetch_error_body)
    download_url_params->set_fetch_error_body(true);
  download_url_params->set_download_source(
      download::DownloadSource::INTERNAL_API);
  download_url_params->set_post_body(post_body);
  download_url_params->set_cross_origin_redirects(
      network::mojom::RedirectMode::kFollow);
  download_url_params->set_upload_progress_callback(
      base::BindRepeating(&DownloadDriverImpl::OnUploadProgress,
                          weak_ptr_factory_.GetWeakPtr(), guid));
  download_url_params->set_require_safety_checks(
      request_params.require_safety_checks);
  if (request_params.isolation_info) {
    download_url_params->set_isolation_info(
        request_params.isolation_info.value());
  }
  download_url_params->set_update_first_party_url_on_redirect(
      request_params.update_first_party_url_on_redirect);

  download_manager_coordinator_->DownloadUrl(std::move(download_url_params));
}

void DownloadDriverImpl::Remove(const std::string& guid, bool remove_file) {
  guid_to_remove_.emplace(guid);

  // DownloadItem::Remove will cause the item object removed from memory, post
  // the remove task to avoid the object being accessed in the same call stack.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadDriverImpl::DoRemoveDownload,
                     weak_ptr_factory_.GetWeakPtr(), guid, remove_file));
}

void DownloadDriverImpl::DoRemoveDownload(const std::string& guid,
                                          bool remove_file) {
  if (!download_manager_coordinator_)
    return;
  DownloadItem* item = download_manager_coordinator_->GetDownloadByGuid(guid);
  // Cancels the download and removes the persisted records in content layer.
  if (item) {
    // Remove the download file for completed download.
    if (remove_file)
      item->DeleteFile(base::DoNothing());
    item->Remove();
  }
}

void DownloadDriverImpl::Pause(const std::string& guid) {
  if (!download_manager_coordinator_)
    return;
  DownloadItem* item = download_manager_coordinator_->GetDownloadByGuid(guid);
  if (item)
    item->Pause();
}

void DownloadDriverImpl::Resume(const std::string& guid) {
  if (!download_manager_coordinator_)
    return;
  DownloadItem* item = download_manager_coordinator_->GetDownloadByGuid(guid);
  if (item)
    item->Resume(true);
}

std::optional<DriverEntry> DownloadDriverImpl::Find(const std::string& guid) {
  if (!download_manager_coordinator_)
    return std::nullopt;
  DownloadItem* item = download_manager_coordinator_->GetDownloadByGuid(guid);
  if (item)
    return CreateDriverEntry(item);
  return std::nullopt;
}

std::set<std::string> DownloadDriverImpl::GetActiveDownloads() {
  std::set<std::string> guids;
  if (!download_manager_coordinator_)
    return guids;

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> items;
  download_manager_coordinator_->GetAllDownloads(&items);

  for (download::DownloadItem* item : items) {
    DriverEntry::State state = ToDriverEntryState(item->GetState());
    if (state == DriverEntry::State::IN_PROGRESS)
      guids.insert(item->GetGuid());
  }

  return guids;
}

size_t DownloadDriverImpl::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(guid_to_remove_);
}

void DownloadDriverImpl::OnDownloadUpdated(
    SimpleDownloadManagerCoordinator* coordinator,
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

void DownloadDriverImpl::OnDownloadRemoved(
    SimpleDownloadManagerCoordinator* coordinator,
    DownloadItem* download) {
  guid_to_remove_.erase(download->GetGuid());
  // |download| is about to be deleted.
}

void DownloadDriverImpl::OnDownloadCreated(
    SimpleDownloadManagerCoordinator* coordinator,
    DownloadItem* item) {
  if (guid_to_remove_.find(item->GetGuid()) != guid_to_remove_.end()) {
    // Client has removed the download before content persistence layer created
    // the record, remove the download immediately.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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

void DownloadDriverImpl::OnUploadProgress(const std::string& guid,
                                          uint64_t bytes_uploaded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnUploadProgress(guid, bytes_uploaded);
}

void DownloadDriverImpl::OnDownloadsInitialized(
    SimpleDownloadManagerCoordinator* coordinator,
    bool active_downloads_only) {
  DCHECK_EQ(download_manager_coordinator_, coordinator);
  DCHECK(download_manager_coordinator_);

  if (!client_)
    return;

  if (is_ready_)
    return;

  client_->OnDriverReady(true);
  is_ready_ = true;
}

void DownloadDriverImpl::OnManagerGoingDown(
    SimpleDownloadManagerCoordinator* coordinator) {
  DCHECK_EQ(download_manager_coordinator_, coordinator);
  download_manager_coordinator_ = nullptr;
}

void DownloadDriverImpl::OnHardRecoverComplete(bool success) {
  client_->OnDriverHardRecoverComplete(success);
}

}  // namespace download
