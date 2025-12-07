// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_fetch/download_client.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/background_fetch/background_fetch_delegate_base.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/download_metadata.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "url/origin.h"

namespace background_fetch {

namespace {

using BackgroundFetchFailureReason =
    content::BackgroundFetchResult::FailureReason;
BackgroundFetchFailureReason ToBackgroundFetchFailureReason(
    download::Client::FailureReason reason) {
  switch (reason) {
    case download::Client::FailureReason::NETWORK:
      return BackgroundFetchFailureReason::NETWORK;
    case download::Client::FailureReason::UPLOAD_TIMEDOUT:
    case download::Client::FailureReason::TIMEDOUT:
      return BackgroundFetchFailureReason::TIMEDOUT;
    case download::Client::FailureReason::UNKNOWN:
      return BackgroundFetchFailureReason::FETCH_ERROR;
    case download::Client::FailureReason::ABORTED:
    case download::Client::FailureReason::CANCELLED:
      return BackgroundFetchFailureReason::CANCELLED;
  }
}

}  // namespace

DownloadClient::DownloadClient(content::BrowserContext* context)
    : browser_context_(context) {}

DownloadClient::~DownloadClient() = default;

void DownloadClient::OnServiceInitialized(
    bool state_lost,
    const std::vector<download::DownloadMetaData>& downloads) {
  std::set<std::string> outstanding_guids =
      GetDelegate()->TakeOutstandingGuids();
  for (const auto& download : downloads) {
    if (!outstanding_guids.count(download.guid)) {
      // Background Fetch is not aware of this GUID, so it successfully
      // completed but the information is still around.
      continue;
    }

    if (download.completion_info) {
      // The download finished but was not persisted.
      OnDownloadSucceeded(download.guid, *download.completion_info);
      return;
    }

    // The download is active, and will call the appropriate functions.

    if (download.paused) {
      // We need to resurface the notification in a paused state.
      content::BrowserThread::PostBestEffortTask(
          FROM_HERE, base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&BackgroundFetchDelegateBase::RestartPausedDownload,
                         GetDelegate()->GetWeakPtr(), download.guid));
    }
  }

  // There is also the case that the Download Service is not aware of the GUID.
  // i.e. there is a guid in |outstanding_guids| not in |downloads|.
  // This can be due to:
  // 1. The browser crashing before the download started.
  // 2. The download failing before persisting the state.
  // 3. The browser was forced to clean-up the the download.
  // In either case the download should be allowed to restart, so there is
  // nothing to do here.
}

void DownloadClient::OnServiceUnavailable() {}

void DownloadClient::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  // TODO(crbug.com/40593934): Validate the chain/headers and cancel the
  // download if invalid.
  auto response =
      std::make_unique<content::BackgroundFetchResponse>(url_chain, headers);
  GetDelegate()->OnDownloadStarted(guid, std::move(response));
}

void DownloadClient::OnDownloadUpdated(const std::string& guid,
                                       uint64_t bytes_uploaded,
                                       uint64_t bytes_downloaded) {
  GetDelegate()->OnDownloadUpdated(guid, bytes_uploaded, bytes_downloaded);
}

void DownloadClient::OnDownloadFailed(const std::string& guid,
                                      const download::CompletionInfo& info,
                                      download::Client::FailureReason reason) {
  auto response = std::make_unique<content::BackgroundFetchResponse>(
      info.url_chain, info.response_headers);
  auto result = std::make_unique<content::BackgroundFetchResult>(
      std::move(response), base::Time::Now(),
      ToBackgroundFetchFailureReason(reason));
  GetDelegate()->OnDownloadFailed(guid, std::move(result));
}

void DownloadClient::OnDownloadSucceeded(const std::string& guid,
                                         const download::CompletionInfo& info) {
  if (browser_context_->IsOffTheRecord()) {
    DCHECK(info.blob_handle);
  } else {
    DCHECK(!info.path.empty());
  }

  auto response = std::make_unique<content::BackgroundFetchResponse>(
      info.url_chain, info.response_headers);
  auto result = std::make_unique<content::BackgroundFetchResult>(
      std::move(response), base::Time::Now(), info.path, info.blob_handle,
      info.bytes_downloaded);

  GetDelegate()->OnDownloadSucceeded(guid, std::move(result));
}

bool DownloadClient::CanServiceRemoveDownloadedFile(const std::string& guid,
                                                    bool force_delete) {
  // If |force_delete| is true the file will be removed anyway.
  // TODO(rayankans): Add UMA to see how often this happens.
  return force_delete || GetDelegate()->IsGuidOutstanding(guid);
}

void DownloadClient::GetUploadData(const std::string& guid,
                                   download::GetUploadDataCallback callback) {
  GetDelegate()->GetUploadData(guid, std::move(callback));
}

BackgroundFetchDelegateBase* DownloadClient::GetDelegate() {
  return static_cast<BackgroundFetchDelegateBase*>(
      browser_context_->GetBackgroundFetchDelegate());
}

}  // namespace background_fetch
