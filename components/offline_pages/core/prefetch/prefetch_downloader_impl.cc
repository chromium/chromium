// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_downloader_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/background_service/service_config.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_server_urls.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

void NotifyDispatcher(PrefetchService* service, PrefetchDownloadResult result) {
  if (service) {
    PrefetchDispatcher* dispatcher = service->GetPrefetchDispatcher();
    if (dispatcher)
      dispatcher->DownloadCompleted(result);
  }
}

}  // namespace

PrefetchDownloaderImpl::PrefetchDownloaderImpl(
    download::DownloadService* download_service,
    version_info::Channel channel,
    PrefService* prefs)
    : download_service_(download_service), channel_(channel), prefs_(prefs) {
  DCHECK(download_service);
}

PrefetchDownloaderImpl::~PrefetchDownloaderImpl() = default;

void PrefetchDownloaderImpl::SetPrefetchService(PrefetchService* service) {
  prefetch_service_ = service;
}

bool PrefetchDownloaderImpl::IsDownloadServiceUnavailable() const {
  return download_service_status_ == DownloadServiceStatus::UNAVAILABLE;
}

void PrefetchDownloaderImpl::CleanupDownloadsWhenReady() {
  // Do nothing if downloads were already cleaned up.
  if (did_download_cleanup_)
    return;

  // Trigger the download cleanup if the download service has already started.
  if (download_service_status_ == DownloadServiceStatus::STARTED) {
    CleanupDownloads(outstanding_download_ids_, success_downloads_);
    return;
  }

  // If the download service has not started, remember that we already were
  // asked to cleanup downloads.
  cleanup_downloads_when_service_starts_ = true;
}

void PrefetchDownloaderImpl::StartDownload(const std::string& download_id,
                                           const std::string& download_location,
                                           const std::string& operation_name) {
  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Start download of '" + download_location +
      "', download_id=" + download_id);

  download::DownloadParams params;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("prefetch_download", R"(
        semantics {
          sender: "Prefetch Downloader"
          description:
            "Chromium interacts with Offline Page Service to prefetch "
            "suggested website resources."
          trigger:
            "When there are suggested website resources to fetch."
          data:
            "The link to the contents of the suggested website resources to "
            "fetch."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable offline prefetch by toggling "
            "'Download articles for you' in settings under Downloads or "
            "by toggling chrome://flags#offline-prefetch."
          chrome_policy {
            NetworkPredictionOptions {
              NetworkPredictionOptions: 2
            }
          }
        })");
  params.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation);
  params.client = download::DownloadClient::OFFLINE_PAGE_PREFETCH;
  params.guid = download_id;
  params.callback = base::AdaptCallbackForRepeating(
      base::BindOnce(&PrefetchDownloaderImpl::OnStartDownload,
                     weak_ptr_factory_.GetWeakPtr()));
  params.scheduling_params.network_requirements =
      download::SchedulingParams::NetworkRequirements::UNMETERED;
  params.scheduling_params.battery_requirements =
      download::SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;
  params.scheduling_params.cancel_time =
      OfflineTimeNow() + kPrefetchDownloadLifetime;
  params.request_params.url = PrefetchDownloadURL(download_location, channel_);
  params.request_params.require_safety_checks = false;

  std::string experiment_header = PrefetchExperimentHeader();
  if (!experiment_header.empty()) {
    params.request_params.request_headers.AddHeaderFromString(
        experiment_header);
  }

  if (!operation_name.empty() &&
      net::HttpUtil::IsValidHeaderValue(operation_name)) {
    params.request_params.request_headers.SetHeader(
        kPrefetchOperationHeaderName, operation_name);
  } else {
    // Offline internals uses operation_name="".
    LOG(WARNING) << "Not setting " << kPrefetchOperationHeaderName
                 << ", invalid operation name '" << operation_name << "'";
  }

  // Lessen download restrictions if limitless prefetching is enabled.
  if (prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs_)) {
    params.scheduling_params.network_requirements =
        download::SchedulingParams::NetworkRequirements::NONE;
    params.scheduling_params.battery_requirements =
        download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
    params.scheduling_params.priority =
        download::SchedulingParams::Priority::HIGH;
  }

  // The download service can queue the download even if it is not fully up yet.
  download_service_->StartDownload(params);
}

void PrefetchDownloaderImpl::OnDownloadServiceReady(
    const std::set<std::string>& outstanding_download_ids,
    const std::map<std::string, std::pair<base::FilePath, int64_t>>&
        success_downloads) {
  DCHECK_EQ(DownloadServiceStatus::INITIALIZING, download_service_status_);
  download_service_status_ = DownloadServiceStatus::STARTED;
  // Given the imposed simultaneous downloads limits, outstanding_download_ids
  // will only ever contain a handful of elements and so only a negligible
  // performance impact is expected from the trace-only loop below.
  for (const std::string& outstanding_download_id : outstanding_download_ids) {
    TRACE_EVENT_ASYNC_BEGIN2(
        "offline_pages", "PrefetchDownloaderImpl: downloading article",
        std::hash<std::string>{}(outstanding_download_id), "download_id",
        outstanding_download_id, "resumed after restart", "true");
  }

  prefetch_service_->GetLogger()->RecordActivity("Downloader: Service ready.");

  // If the prefetch service has requested the download cleanup, do it now.
  if (cleanup_downloads_when_service_starts_) {
    CleanupDownloads(outstanding_download_ids, success_downloads);
    return;
  }

  // Otherwise, cache the download cleanup data until told by the prefetch
  // service.
  outstanding_download_ids_ = outstanding_download_ids;
  success_downloads_ = success_downloads;
}

void PrefetchDownloaderImpl::OnDownloadServiceUnavailable() {
  DCHECK_EQ(DownloadServiceStatus::INITIALIZING, download_service_status_);
  download_service_status_ = DownloadServiceStatus::UNAVAILABLE;

  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Service unavailable.");

  // The download service is unavailable to use for the whole lifetime of
  // Chrome. PrefetchService can't schedule any downloads. Next time when Chrome
  // restarts, the download service might be back to operational.
}

void PrefetchDownloaderImpl::OnDownloadSucceeded(
    const std::string& download_id,
    const base::FilePath& file_path,
    int64_t file_size) {
  TRACE_EVENT_ASYNC_END1(
      "offline_pages", "PrefetchDownloaderImpl: downloading article",
      std::hash<std::string>{}(download_id), "succeeded", "true");
  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Download succeeded, download_id=" + download_id);
  NotifyDispatcher(prefetch_service_,
                   PrefetchDownloadResult(download_id, file_path, file_size));
}

void PrefetchDownloaderImpl::OnDownloadFailed(const std::string& download_id) {
  TRACE_EVENT_ASYNC_END1(
      "offline_pages", "PrefetchDownloaderImpl: downloading article",
      std::hash<std::string>{}(download_id), "succeeded", "false");
  PrefetchDownloadResult result;
  result.download_id = download_id;
  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Download failed, download_id=" + download_id);
  NotifyDispatcher(prefetch_service_, result);
}

int PrefetchDownloaderImpl::GetMaxConcurrentDownloads() {
  return download_service_->GetConfig().GetMaxConcurrentDownloads();
}

void PrefetchDownloaderImpl::OnStartDownload(
    const std::string& download_id,
    download::DownloadParams::StartResult result) {
  prefetch_service_->GetLogger()->RecordActivity(
      "Downloader: Download started, download_id=" + download_id +
      ", result=" + std::to_string(static_cast<int>(result)));
  // Treat the non-accepted request to start a download as an ordinary failure
  // to simplify the control flow since this situation should rarely happen. The
  // Download.Service.Request.StartResult.OfflinePage histogram tracks these
  // cases and would signal the need to revisit this decision.
  if (result != download::DownloadParams::StartResult::ACCEPTED) {
    OnDownloadFailed(download_id);
  } else {
    TRACE_EVENT_ASYNC_BEGIN1(
        "offline_pages", "PrefetchDownloaderImpl: downloading article",
        std::hash<std::string>{}(download_id), "download_id", download_id);
  }
}

void PrefetchDownloaderImpl::CleanupDownloads(
    const std::set<std::string>& outstanding_download_ids,
    const std::map<std::string, std::pair<base::FilePath, int64_t>>&
        success_downloads) {
  // Prevent future cleanup by marking |did_download_cleanup_|.
  did_download_cleanup_ = true;
  cleanup_downloads_when_service_starts_ = false;

  PrefetchDispatcher* dispatcher = prefetch_service_->GetPrefetchDispatcher();
  if (dispatcher)
    dispatcher->CleanupDownloads(outstanding_download_ids, success_downloads);
}

}  // namespace offline_pages
