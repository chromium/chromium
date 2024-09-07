// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_fetch/background_fetch_delegate_base.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/background_fetch/job_details.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/blob_context_getter_factory.h"
#include "components/download/public/background_service/download_params.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace background_fetch {

BackgroundFetchDelegateBase::BackgroundFetchDelegateBase(
    content::BrowserContext* context)
    : context_(context) {}

BackgroundFetchDelegateBase::~BackgroundFetchDelegateBase() = default;

void BackgroundFetchDelegateBase::GetIconDisplaySize(
    BackgroundFetchDelegate::GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If Android, return 192x192, else return 0x0. 0x0 means not loading an
  // icon at all, which is returned for all non-Android platforms as the
  // icons can't be displayed on the UI yet.
  gfx::Size display_size;
#if BUILDFLAG(IS_ANDROID)
  display_size = gfx::Size(192, 192);
#endif
  std::move(callback).Run(display_size);
}

void BackgroundFetchDelegateBase::CreateDownloadJob(
    base::WeakPtr<Client> client,
    std::unique_ptr<content::BackgroundFetchDescription> fetch_description) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const std::string job_id = fetch_description->job_unique_id;

  auto inserted = job_details_map_.emplace(std::piecewise_construct,
                                           std::forward_as_tuple(job_id),
                                           std::forward_as_tuple());
  DCHECK(inserted.second);
  JobDetails* job_details = &inserted.first->second;
  job_details->client = std::move(client);

  job_details->job_state =
      fetch_description->start_paused
          ? JobDetails::State::kPendingWillStartPaused
          : JobDetails::State::kPendingWillStartDownloading;

  job_details->fetch_description = std::move(fetch_description);

  OnJobDetailsCreated(job_id);
}

void BackgroundFetchDelegateBase::DownloadUrl(
    const std::string& job_id,
    const std::string& download_guid,
    const std::string& method,
    const GURL& url,
    ::network::mojom::CredentialsMode credentials_mode,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const net::HttpRequestHeaders& headers,
    bool has_request_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!download_job_id_map_.count(download_guid));

  download_job_id_map_.emplace(download_guid, job_id);

  download::DownloadParams params;
  params.guid = download_guid;
  params.client = download::DownloadClient::BACKGROUND_FETCH;
  params.request_params.method = method;
  params.request_params.url = url;
  params.request_params.request_headers = headers;
  params.request_params.credentials_mode = credentials_mode;
  params.callback =
      base::BindRepeating(&BackgroundFetchDelegateBase::OnDownloadReceived,
                          weak_ptr_factory_.GetWeakPtr());
  params.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation);
  params.request_params.update_first_party_url_on_redirect = false;

  JobDetails* job_details = GetJobDetails(job_id);
  if (job_details->job_state == JobDetails::State::kPendingWillStartPaused ||
      job_details->job_state ==
          JobDetails::State::kPendingWillStartDownloading) {
    DoShowUi(job_id);
    job_details->MarkJobAsStarted();
  }

  params.request_params.isolation_info =
      job_details->fetch_description->isolation_info;

  if (job_details->job_state == JobDetails::State::kStartedButPaused) {
    job_details->on_resume = base::BindOnce(
        &BackgroundFetchDelegateBase::StartDownload, GetWeakPtr(), job_id,
        std::move(params), has_request_body);
  } else {
    StartDownload(job_id, std::move(params), has_request_body);
  }

  DoUpdateUi(job_id);
}

void BackgroundFetchDelegateBase::PauseDownload(const std::string& job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JobDetails* job_details = GetJobDetails(job_id, /*allow_null=*/true);
  if (!job_details) {
    return;
  }

  if (job_details->job_state == JobDetails::State::kDownloadsComplete ||
      job_details->job_state == JobDetails::State::kJobComplete) {
    // The pause event arrived after the fetch was complete; ignore it.
    return;
  }

  job_details->job_state = JobDetails::State::kStartedButPaused;
  for (auto& download_guid_pair : job_details->current_fetch_guids)
    GetDownloadService()->PauseDownload(download_guid_pair.first);
}

void BackgroundFetchDelegateBase::ResumeDownload(const std::string& job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JobDetails* job_details = GetJobDetails(job_id, /*allow_null=*/true);
  if (!job_details)
    return;

  job_details->job_state = JobDetails::State::kStartedAndDownloading;
  for (auto& download_guid_pair : job_details->current_fetch_guids)
    GetDownloadService()->ResumeDownload(download_guid_pair.first);

  if (job_details->on_resume)
    std::move(job_details->on_resume).Run();
}

void BackgroundFetchDelegateBase::CancelDownload(std::string job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JobDetails* job_details = GetJobDetails(job_id);

  if (!job_details ||
      job_details->job_state == JobDetails::State::kDownloadsComplete ||
      job_details->job_state == JobDetails::State::kJobComplete) {
    // The cancel event arrived after the fetch was complete; ignore it.
    return;
  }

  job_details->cancelled_from_ui = true;
  Abort(job_id);

  if (auto client = GetClient(job_id)) {
    // The |download_guid| is not releavnt here as the job has already
    // been aborted and is assumed to have been removed.
    client->OnJobCancelled(
        job_id, "" /* download_guid */,
        blink::mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI);
  }
}

void BackgroundFetchDelegateBase::OnUiFinished(const std::string& job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  job_details_map_.erase(job_id);
  DoCleanUpUi(job_id);
}

void BackgroundFetchDelegateBase::OnUiActivated(const std::string& job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto client = GetClient(job_id))
    client->OnUIActivated(job_id);
}

JobDetails* BackgroundFetchDelegateBase::GetJobDetails(
    const std::string& job_id,
    bool allow_null) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto job_details_iter = job_details_map_.find(job_id);
  if (job_details_iter == job_details_map_.end()) {
    if (!allow_null) {
      NOTREACHED_IN_MIGRATION();
    }

    return nullptr;
  }
  return &job_details_iter->second;
}

void BackgroundFetchDelegateBase::StartDownload(const std::string& job_id,
                                                download::DownloadParams params,
                                                bool has_request_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetJobDetails(job_id)->current_fetch_guids.emplace(params.guid,
                                                     has_request_body);
  GetDownloadService()->StartDownload(std::move(params));
}

void BackgroundFetchDelegateBase::Abort(const std::string& job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  JobDetails* job_details = GetJobDetails(job_id, /*allow_null=*/true);
  if (!job_details) {
    return;
  }

  job_details->job_state = JobDetails::State::kCancelled;

  for (const auto& download_guid_pair : job_details->current_fetch_guids) {
    GetDownloadService()->CancelDownload(download_guid_pair.first);
    download_job_id_map_.erase(download_guid_pair.first);
  }
  DoUpdateUi(job_id);
}

void BackgroundFetchDelegateBase::MarkJobComplete(const std::string& job_id) {
  JobDetails* job_details = GetJobDetails(job_id);

  if (job_details->job_state == JobDetails::State::kCancelled) {
    OnUiFinished(job_id);
    return;
  }

  job_details->job_state = JobDetails::State::kJobComplete;

  // Clear the |job_details| internals that are no longer needed.
  job_details->current_fetch_guids.clear();
}

void BackgroundFetchDelegateBase::FailFetch(const std::string& job_id,
                                            const std::string& download_guid) {
  // Save a copy before Abort() deletes the reference.
  const std::string unique_id = job_id;
  Abort(job_id);

  if (auto client = GetClient(unique_id)) {
    client->OnJobCancelled(
        download_guid, unique_id,
        blink::mojom::BackgroundFetchFailureReason::DOWNLOAD_TOTAL_EXCEEDED);
  }
}

void BackgroundFetchDelegateBase::OnDownloadStarted(
    const std::string& download_guid,
    std::unique_ptr<content::BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto download_job_id_iter = download_job_id_map_.find(download_guid);
  // TODO(crbug.com/40546930): When DownloadService fixes cancelled jobs calling
  // OnDownload* methods, then this can be a DCHECK.
  if (download_job_id_iter == download_job_id_map_.end()) {
    return;
  }

  const std::string& job_id = download_job_id_iter->second;
  JobDetails* job_details = GetJobDetails(job_id);
  if (job_details->client) {
    job_details->client->OnDownloadStarted(job_id, download_guid,
                                           std::move(response));
  }

  // Update the upload progress.
  auto it = job_details->current_fetch_guids.find(download_guid);
  CHECK(it != job_details->current_fetch_guids.end(),
        base::NotFatalUntil::M130);
  job_details->fetch_description->uploaded_bytes += it->second.body_size_bytes;
}

void BackgroundFetchDelegateBase::OnDownloadUpdated(
    const std::string& download_guid,
    uint64_t bytes_uploaded,
    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto download_job_id_iter = download_job_id_map_.find(download_guid);
  // TODO(crbug.com/40546930): When DownloadService fixes cancelled jobs calling
  // OnDownload* methods, then this can be a DCHECK.
  if (download_job_id_iter == download_job_id_map_.end()) {
    return;
  }

  const std::string job_id = download_job_id_iter->second;

  JobDetails* job_details = GetJobDetails(job_id);
  job_details->UpdateInProgressBytes(download_guid, bytes_uploaded,
                                     bytes_downloaded);
  if (job_details->fetch_description->download_total_bytes &&
      job_details->fetch_description->download_total_bytes <
          job_details->GetDownloadedBytes()) {
    // Fail the fetch if total download size was set too low.
    // We only do this if total download size is specified. If not specified,
    // this check is skipped. This is to allow for situations when the
    // total download size cannot be known when invoking fetch.
    FailFetch(job_id, download_guid);
    return;
  }
  DoUpdateUi(job_id);

  if (job_details->client) {
    job_details->client->OnDownloadUpdated(job_id, download_guid,
                                           bytes_uploaded, bytes_downloaded);
  }
}

void BackgroundFetchDelegateBase::OnDownloadFailed(
    const std::string& download_guid,
    std::unique_ptr<content::BackgroundFetchResult> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto download_job_id_iter = download_job_id_map_.find(download_guid);
  // TODO(crbug.com/40546930): When DownloadService fixes cancelled jobs
  // potentially calling OnDownloadFailed with a reason other than
  // CANCELLED/ABORTED, we should add a DCHECK here.
  if (download_job_id_iter == download_job_id_map_.end()) {
    return;
  }

  const std::string& job_id = download_job_id_iter->second;
  JobDetails* job_details = GetJobDetails(job_id);
  job_details->UpdateJobOnDownloadComplete(download_guid);
  DoUpdateUi(job_id);

  // The client cancelled or aborted the download so no need to notify it.
  if (result->failure_reason ==
      content::BackgroundFetchResult::FailureReason::CANCELLED) {
    return;
  }

  if (job_details->client) {
    job_details->client->OnDownloadComplete(job_id, download_guid,
                                            std::move(result));
  }

  download_job_id_map_.erase(download_guid);
}

void BackgroundFetchDelegateBase::OnDownloadSucceeded(
    const std::string& download_guid,
    std::unique_ptr<content::BackgroundFetchResult> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto download_job_id_iter = download_job_id_map_.find(download_guid);
  // TODO(crbug.com/40546930): When DownloadService fixes cancelled jobs calling
  // OnDownload* methods, then this can be a DCHECK.
  if (download_job_id_iter == download_job_id_map_.end()) {
    return;
  }

  const std::string& job_id = download_job_id_iter->second;
  JobDetails* job_details = GetJobDetails(job_id);
  job_details->UpdateJobOnDownloadComplete(download_guid);

  job_details->fetch_description->downloaded_bytes +=
      context_->IsOffTheRecord() ? result->blob_handle->size()
                                 : result->file_size;

  DoUpdateUi(job_id);

  if (job_details->client) {
    job_details->client->OnDownloadComplete(job_id, download_guid,
                                            std::move(result));
  }

  download_job_id_map_.erase(download_guid);
}

void BackgroundFetchDelegateBase::OnDownloadReceived(
    const std::string& download_guid,
    download::DownloadParams::StartResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using StartResult = download::DownloadParams::StartResult;
  switch (result) {
    case StartResult::ACCEPTED:
      // Nothing to do.
      break;
    case StartResult::UNEXPECTED_GUID:
      // The download started in a previous session. Nothing to do.
      break;
    case StartResult::BACKOFF:
      // TODO(delphick): try again later?
      NOTREACHED_IN_MIGRATION();
      break;
    case StartResult::UNEXPECTED_CLIENT:
      // This really should never happen since we're supplying the
      // DownloadClient.
      NOTREACHED_IN_MIGRATION();
      break;
    case StartResult::CLIENT_CANCELLED:
      // TODO(delphick): do we need to do anything here, since we will have
      // cancelled it?
      break;
    case StartResult::INTERNAL_ERROR:
      // TODO(delphick): We need to handle this gracefully.
      NOTREACHED_IN_MIGRATION();
      break;
    case StartResult::COUNT:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

bool BackgroundFetchDelegateBase::IsGuidOutstanding(
    const std::string& guid) const {
  auto job_id_iter = download_job_id_map_.find(guid);
  if (job_id_iter == download_job_id_map_.end()) {
    return false;
  }

  auto job_details_iter = job_details_map_.find(job_id_iter->second);
  if (job_details_iter == job_details_map_.end()) {
    return false;
  }

  return base::Contains(
      job_details_iter->second.fetch_description->outstanding_guids, guid);
}

void BackgroundFetchDelegateBase::RestartPausedDownload(
    const std::string& download_guid) {
  auto job_it = download_job_id_map_.find(download_guid);

  if (job_it == download_job_id_map_.end()) {
    return;
  }

  const std::string& job_id = job_it->second;

  GetJobDetails(job_id)->job_state = JobDetails::State::kStartedButPaused;

  DoUpdateUi(job_id);
}

std::set<std::string> BackgroundFetchDelegateBase::TakeOutstandingGuids() {
  std::set<std::string> outstanding_guids;
  for (auto& job_id_details : job_details_map_) {
    auto& job_details = job_id_details.second;

    // If the job is loaded at this point, then it already started
    // in a previous session.
    job_details.MarkJobAsStarted();

    std::vector<std::string>& job_outstanding_guids =
        job_details.fetch_description->outstanding_guids;
    for (std::string& outstanding_guid : job_outstanding_guids)
      outstanding_guids.insert(std::move(outstanding_guid));
    job_outstanding_guids.clear();
  }
  return outstanding_guids;
}

void BackgroundFetchDelegateBase::GetUploadData(
    const std::string& download_guid,
    download::GetUploadDataCallback callback) {
  auto job_it = download_job_id_map_.find(download_guid);
  // TODO(crbug.com/40546930): When DownloadService fixes cancelled jobs calling
  // client methods, then this can be a DCHECK.
  if (job_it == download_job_id_map_.end()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /* request_body= */ nullptr));
    return;
  }

  const std::string& job_id = job_it->second;
  JobDetails* job_details = GetJobDetails(job_id);
  if (job_details->current_fetch_guids.at(download_guid).status ==
      JobDetails::RequestData::Status::kAbsent) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /* request_body= */ nullptr));
    return;
  }

  if (job_details->client) {
    job_details->client->GetUploadData(
        job_id, download_guid,
        base::BindOnce(&BackgroundFetchDelegateBase::DidGetUploadData,
                       weak_ptr_factory_.GetWeakPtr(), job_id, download_guid,
                       std::move(callback)));
  }
}

void BackgroundFetchDelegateBase::DidGetUploadData(
    const std::string& job_id,
    const std::string& download_guid,
    download::GetUploadDataCallback callback,
    blink::mojom::SerializedBlobPtr blob) {
  if (!blob || blob->uuid.empty()) {
    std::move(callback).Run(/* request_body= */ nullptr);
    return;
  }

  JobDetails* job_details = GetJobDetails(job_id, /*allow_null=*/true);
  if (!job_details) {
    std::move(callback).Run(/* request_body= */ nullptr);
    return;
  }

  DCHECK(job_details->current_fetch_guids.count(download_guid));
  auto& request_data = job_details->current_fetch_guids.at(download_guid);
  request_data.body_size_bytes = blob->size;

  // Use a Data Pipe to transfer the blob.
  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter_remote;
  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob->blob));
  blob_remote->AsDataPipeGetter(
      data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
  auto request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  request_body->AppendDataPipe(std::move(data_pipe_getter_remote));

  std::move(callback).Run(request_body);
}

base::WeakPtr<content::BackgroundFetchDelegate::Client>
BackgroundFetchDelegateBase::GetClient(const std::string& job_id) {
  auto it = job_details_map_.find(job_id);
  if (it == job_details_map_.end()) {
    return nullptr;
  }
  return it->second.client;
}

}  // namespace background_fetch
