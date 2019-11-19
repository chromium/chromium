// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_importer.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/server_forbidden_check_request.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "components/offline_pages/core/prefetch/suggestions_provider.h"
#include "components/offline_pages/core/prefetch/tasks/add_unique_urls_task.h"
#include "components/offline_pages/core/prefetch/tasks/download_archives_task.h"
#include "components/offline_pages/core/prefetch/tasks/download_cleanup_task.h"
#include "components/offline_pages/core/prefetch/tasks/download_completed_task.h"
#include "components/offline_pages/core/prefetch/tasks/finalize_dismissed_url_suggestion_task.h"
#include "components/offline_pages/core/prefetch/tasks/generate_page_bundle_reconcile_task.h"
#include "components/offline_pages/core/prefetch/tasks/generate_page_bundle_task.h"
#include "components/offline_pages/core/prefetch/tasks/get_operation_task.h"
#include "components/offline_pages/core/prefetch/tasks/import_archives_task.h"
#include "components/offline_pages/core/prefetch/tasks/import_cleanup_task.h"
#include "components/offline_pages/core/prefetch/tasks/import_completed_task.h"
#include "components/offline_pages/core/prefetch/tasks/mark_operation_done_task.h"
#include "components/offline_pages/core/prefetch/tasks/metrics_finalization_task.h"
#include "components/offline_pages/core/prefetch/tasks/page_bundle_update_task.h"
#include "components/offline_pages/core/prefetch/tasks/remove_url_task.h"
#include "components/offline_pages/core/prefetch/tasks/sent_get_operation_cleanup_task.h"
#include "components/offline_pages/core/prefetch/tasks/stale_entry_finalizer_task.h"
#include "components/offline_pages/core/prefetch/thumbnail_fetcher.h"
#include "components/offline_pages/core/prefetch/visuals_fetch_by_url.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {

void DeleteBackgroundTaskHelper(std::unique_ptr<PrefetchBackgroundTask> task) {
  task.reset();
}

PrefetchURL SuggestionToPrefetchURL(PrefetchSuggestion suggestion) {
  return PrefetchURL(suggestion.article_url.spec(), suggestion.article_url,
                     base::UTF8ToUTF16(suggestion.article_title),
                     suggestion.thumbnail_url, suggestion.favicon_url,
                     suggestion.article_snippet,
                     suggestion.article_attribution);
}

}  // namespace

PrefetchDispatcherImpl::PrefetchDispatcherImpl(PrefService* pref_service)
    : pref_service_(pref_service), task_queue_(this) {}

PrefetchDispatcherImpl::~PrefetchDispatcherImpl() = default;

void PrefetchDispatcherImpl::SetService(PrefetchService* service) {
  CHECK(service);
  service_ = service;
}

void PrefetchDispatcherImpl::SchedulePipelineProcessing() {
  needs_pipeline_processing_ = true;
  service_->GetLogger()->RecordActivity(
      "Dispatcher: Scheduled more pipeline processing.");
}

void PrefetchDispatcherImpl::EnsureTaskScheduled() {
  if (background_task_) {
    background_task_->SetReschedule(
        PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITHOUT_BACKOFF);
  } else {
    service_->GetPrefetchBackgroundTaskHandler()->EnsureTaskScheduled();
  }
}

void PrefetchDispatcherImpl::AddCandidatePrefetchURLs(
    const std::string& name_space,
    const std::vector<PrefetchURL>& prefetch_urls) {
  if (!prefetch_prefs::IsEnabled(pref_service_)) {
    if (prefetch_prefs::IsForbiddenCheckDue(pref_service_))
      CheckIfEnabledByServer(pref_service_, service_);
    return;
  }

  service_->GetLogger()->RecordActivity("Dispatcher: Received " +
                                        std::to_string(prefetch_urls.size()) +
                                        " suggested URLs.");

  PrefetchStore* prefetch_store = service_->GetPrefetchStore();

  // Run 2 pipeline expiration tasks first to ensure there is no buildup of URLs
  // in the pipeline if the new ones are coming but NOW can't be entered (for
  // example, if the user is never on WiFi with enough battery charge).
  // First, detect stale entries and move them to FINISHED.
  task_queue_.AddTask(
      std::make_unique<StaleEntryFinalizerTask>(this, prefetch_store));

  // Second, move FINISHED to ZOMBIE.
  task_queue_.AddTask(
      std::make_unique<MetricsFinalizationTask>(prefetch_store));

  // Third, add new unique URLs and remove unneeded ZOMBIEs.
  std::unique_ptr<Task> add_task = std::make_unique<AddUniqueUrlsTask>(
      this, prefetch_store, name_space, prefetch_urls);
  task_queue_.AddTask(std::move(add_task));

  // Report the 'enabled' day if we receive URLs and Prefetch is enabled.
  service_->GetOfflineMetricsCollector()->OnPrefetchEnabled();
}

void PrefetchDispatcherImpl::NewSuggestionsAvailable(
    SuggestionsProvider* suggestions_provider) {
  // No need to GetKnownContent if prefetching is disabled (however, we don't
  // want to prevent the server-forbidden check).
  if (!prefetch_prefs::IsEnabled(pref_service_)) {
    if (prefetch_prefs::IsForbiddenCheckDue(pref_service_))
      CheckIfEnabledByServer(pref_service_, service_);
    return;
  }

  suggestions_provider->GetCurrentArticleSuggestions(
      base::BindOnce(&PrefetchDispatcherImpl::AddSuggestions, GetWeakPtr()));
}

void PrefetchDispatcherImpl::RemoveSuggestion(const GURL& url) {
  if (!prefetch_prefs::IsEnabled(pref_service_))
    return;

  // Remove the URL from the prefetch database.
  task_queue_.AddTask(MakeRemoveUrlTask(service_->GetPrefetchStore(), url));

  // Remove the URL from the offline model.
  PageCriteria criteria;
  criteria.url = url;
  criteria.client_namespaces =
      std::vector<std::string>{kSuggestedArticlesNamespace};
  service_->GetOfflinePageModel()->DeletePagesWithCriteria(criteria,
                                                           base::DoNothing());
}

void PrefetchDispatcherImpl::RemoveAllUnprocessedPrefetchURLs(
    const std::string& name_space) {
  if (!prefetch_prefs::IsEnabled(pref_service_))
    return;

  NOTIMPLEMENTED();
}

void PrefetchDispatcherImpl::RemovePrefetchURLsByClientId(
    const ClientId& client_id) {
  if (!prefetch_prefs::IsEnabled(pref_service_))
    return;
  PrefetchStore* prefetch_store = service_->GetPrefetchStore();
  task_queue_.AddTask(std::make_unique<FinalizeDismissedUrlSuggestionTask>(
      prefetch_store, client_id));
}

void PrefetchDispatcherImpl::BeginBackgroundTask(
    std::unique_ptr<PrefetchBackgroundTask> background_task) {
  if (!prefetch_prefs::IsEnabled(pref_service_))
    return;
  service_->GetLogger()->RecordActivity(
      "Dispatcher: Beginning background task.");

  background_task_ = std::move(background_task);
  service_->GetPrefetchBackgroundTaskHandler()->RemoveSuspension();

  // Reset suspended state in case that it was set last time and Chrome is still
  // running till new background task starts after the suspension period.
  suspended_ = false;

  QueueReconcileTasks();
  QueueActionTasks();
}

void PrefetchDispatcherImpl::QueueReconcileTasks() {
  if (suspended_)
    return;

  service_->GetLogger()->RecordActivity("Dispatcher: Adding reconcile tasks.");
  // Note: For optimal results StaleEntryFinalizerTask should be executed before
  // other reconciler tasks that deal with external systems so that entries
  // finalized by it will promptly effect any external processing they relate
  // to.
  task_queue_.AddTask(std::make_unique<StaleEntryFinalizerTask>(
      this, service_->GetPrefetchStore()));

  task_queue_.AddTask(std::make_unique<GeneratePageBundleReconcileTask>(
      service_->GetPrefetchStore(),
      service_->GetPrefetchNetworkRequestFactory()));

  task_queue_.AddTask(std::make_unique<SentGetOperationCleanupTask>(
      service_->GetPrefetchStore(),
      service_->GetPrefetchNetworkRequestFactory()));

  // Notifies the downloader that the download cleanup can proceed when the
  // download service is up. The prefetch service and download service are two
  // separate services which can start up on their own. The download cleanup
  // should only kick in when both services are ready.
  service_->GetPrefetchDownloader()->CleanupDownloadsWhenReady();

  task_queue_.AddTask(std::make_unique<ImportCleanupTask>(
      service_->GetPrefetchStore(), service_->GetPrefetchImporter()));

  // This task should be last, because it is least important for correct
  // operation of the system, and because any reconciliation tasks might
  // generate more entries in the FINISHED state that the finalization task
  // could pick up.
  task_queue_.AddTask(
      std::make_unique<MetricsFinalizationTask>(service_->GetPrefetchStore()));
}

void PrefetchDispatcherImpl::QueueActionTasks() {
  service_->GetLogger()->RecordActivity("Dispatcher: Adding action tasks.");

  // Import should be run first to minimize time to import after download
  // finishes, during the download background task.
  std::unique_ptr<Task> import_archives_task =
      std::make_unique<ImportArchivesTask>(service_->GetPrefetchStore(),
                                           service_->GetPrefetchImporter());
  task_queue_.AddTask(std::move(import_archives_task));

  std::unique_ptr<Task> download_archives_task =
      std::make_unique<DownloadArchivesTask>(service_->GetPrefetchStore(),
                                             service_->GetPrefetchDownloader(),
                                             pref_service_);
  task_queue_.AddTask(std::move(download_archives_task));

  // The following tasks should not be run unless we are in the background task,
  // as we need to ensure WiFi access at that time.
  if (!background_task_)
    return;
  DCHECK(!service_->GetCachedGCMToken().empty());

  std::unique_ptr<Task> get_operation_task = std::make_unique<GetOperationTask>(
      service_->GetPrefetchStore(),
      service_->GetPrefetchNetworkRequestFactory(),
      base::BindRepeating(
          &PrefetchDispatcherImpl::DidGenerateBundleOrGetOperationRequest,
          GetWeakPtr(), "GetOperationRequest"));
  task_queue_.AddTask(std::move(get_operation_task));

  std::unique_ptr<Task> generate_page_bundle_task =
      std::make_unique<GeneratePageBundleTask>(
          this, service_->GetPrefetchStore(), service_->GetCachedGCMToken(),
          service_->GetPrefetchNetworkRequestFactory(),
          base::BindRepeating(
              &PrefetchDispatcherImpl::DidGenerateBundleOrGetOperationRequest,
              GetWeakPtr(), "GeneratePageBundleRequest"));
  task_queue_.AddTask(std::move(generate_page_bundle_task));
}

void PrefetchDispatcherImpl::AddSuggestions(
    std::vector<PrefetchSuggestion> suggestions) {
  std::vector<PrefetchURL> urls;
  urls.reserve(suggestions.size());

  for (auto& suggestion : suggestions) {
    urls.push_back(SuggestionToPrefetchURL(std::move(suggestion)));
  }
  AddCandidatePrefetchURLs(kSuggestedArticlesNamespace, urls);
}

void PrefetchDispatcherImpl::StopBackgroundTask() {
  if (!prefetch_prefs::IsEnabled(pref_service_))
    return;

  service_->GetLogger()->RecordActivity(
      "Dispatcher: Stopping background task.");

  DisposeTask();
}

void PrefetchDispatcherImpl::OnTaskQueueIsIdle() {
  if (!suspended_ && needs_pipeline_processing_) {
    needs_pipeline_processing_ = false;
    QueueActionTasks();
  } else {
    PrefetchNetworkRequestFactory* request_factory =
        service_->GetPrefetchNetworkRequestFactory();
    if (!request_factory->HasOutstandingRequests())
      DisposeTask();
  }
}

void PrefetchDispatcherImpl::DisposeTask() {
  if (!background_task_)
    return;

  // Delay the deletion till the caller finishes.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteBackgroundTaskHelper, std::move(background_task_)));
}

void PrefetchDispatcherImpl::GCMOperationCompletedMessageReceived(
    const std::string& operation_name) {
  if (!prefetch_prefs::IsEnabled(pref_service_))
    return;

  service_->GetLogger()->RecordActivity("Dispatcher: Received GCM message.");

  PrefetchStore* prefetch_store = service_->GetPrefetchStore();
  task_queue_.AddTask(std::make_unique<MarkOperationDoneTask>(
      this, prefetch_store, operation_name));
}

void PrefetchDispatcherImpl::DidGenerateBundleOrGetOperationRequest(
    const std::string& request_name_for_logging,
    PrefetchRequestStatus status,
    const std::string& operation_name,
    const std::vector<RenderPageInfo>& pages) {
  LogRequestResult(request_name_for_logging, status, operation_name, pages);

  // Note that we still want to trigger PageBundleUpdateTask even if the request
  // fails and no page is returned. This is because currently we only check for
  // the empty task queue and no outstanding request in order to decide whether
  // to dispose th background task upon the completion of a task.
  PrefetchStore* prefetch_store = service_->GetPrefetchStore();
  task_queue_.AddTask(std::make_unique<PageBundleUpdateTask>(
      prefetch_store, this, operation_name, pages));

  if (background_task_ && status != PrefetchRequestStatus::kSuccess &&
      status != PrefetchRequestStatus::kEmptyRequestSuccess) {
    PrefetchBackgroundTaskRescheduleType reschedule_type =
        PrefetchBackgroundTaskRescheduleType::NO_RESCHEDULE;
    switch (status) {
      case PrefetchRequestStatus::kShouldRetryWithBackoff:
        reschedule_type =
            PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITH_BACKOFF;
        break;
      case PrefetchRequestStatus::kShouldRetryWithoutBackoff:
        reschedule_type =
            PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITHOUT_BACKOFF;
        break;
      case PrefetchRequestStatus::kShouldSuspendForbiddenByOPS:
      case PrefetchRequestStatus::kShouldSuspendNewlyForbiddenByOPS:
      case PrefetchRequestStatus::kShouldSuspendForbidden:
      case PrefetchRequestStatus::kShouldSuspendNotImplemented:
      case PrefetchRequestStatus::kShouldSuspendBlockedByAdministrator:
        reschedule_type = PrefetchBackgroundTaskRescheduleType::SUSPEND;
        break;
      case PrefetchRequestStatus::kSuccess:
      case PrefetchRequestStatus::kEmptyRequestSuccess:
        NOTREACHED();
        break;
    }
    background_task_->SetReschedule(reschedule_type);

    if (reschedule_type == PrefetchBackgroundTaskRescheduleType::SUSPEND)
      suspended_ = true;
  }
}

void PrefetchDispatcherImpl::CleanupDownloads(
    const std::set<std::string>& outstanding_download_ids,
    const std::map<std::string, std::pair<base::FilePath, int64_t>>&
        success_downloads) {
  task_queue_.AddTask(std::make_unique<DownloadCleanupTask>(
      this, service_->GetPrefetchStore(), outstanding_download_ids,
      success_downloads));
}

void PrefetchDispatcherImpl::GeneratePageBundleRequested(
    std::unique_ptr<PrefetchDispatcher::IdsVector> ids) {
  // Reverse the order so that the fresher items are last. This is done because
  // the ids are popped from the end of the vector.
  std::reverse(ids->begin(), ids->end());
  FetchVisuals(std::move(ids), /* is_first_attempt= */ true);
}

void PrefetchDispatcherImpl::DownloadCompleted(
    const PrefetchDownloadResult& download_result) {
  if (!prefetch_prefs::IsEnabled(pref_service_))
    return;

  service_->GetLogger()->RecordActivity(
      "Download " + download_result.download_id +
      (download_result.success ? "succeeded" : "failed"));
  if (download_result.success) {
    service_->GetLogger()->RecordActivity(
        "Download size: " + std::to_string(download_result.file_size));
  }

  task_queue_.AddTask(std::make_unique<DownloadCompletedTask>(
      this, service_->GetPrefetchStore(), download_result));
  task_queue_.AddTask(std::make_unique<ImportArchivesTask>(
      service_->GetPrefetchStore(), service_->GetPrefetchImporter()));
}

void PrefetchDispatcherImpl::ItemDownloaded(int64_t offline_id,
                                            const ClientId& client_id) {
  auto ids = std::make_unique<IdsVector>();
  ids->emplace_back(offline_id, client_id);
  FetchVisuals(std::move(ids), /* is_first_attempt= */ false);
}

void PrefetchDispatcherImpl::ArchiveImported(int64_t offline_id, bool success) {
  DCHECK_NE(OfflinePageModel::kInvalidOfflineId, offline_id);

  if (!prefetch_prefs::IsEnabled(pref_service_))
    return;

  service_->GetLogger()->RecordActivity("Importing archive " +
                                        std::to_string(offline_id) +
                                        (success ? "succeeded" : "failed"));

  if (success)
    service_->GetOfflineMetricsCollector()->OnSuccessfulPagePrefetch();

  task_queue_.AddTask(std::make_unique<ImportCompletedTask>(
      this, service_->GetPrefetchStore(), service_->GetPrefetchImporter(),
      offline_id, success));
}

void PrefetchDispatcherImpl::LogRequestResult(
    const std::string& request_name_for_logging,
    PrefetchRequestStatus status,
    const std::string& operation_name,
    const std::vector<RenderPageInfo>& pages) {
  service_->GetLogger()->RecordActivity(
      "Finished " + request_name_for_logging +
      " for operation: " + operation_name +
      " with status: " + std::to_string(static_cast<int>(status)) +
      "; included " + std::to_string(pages.size()) + " pages in result.");
  for (const RenderPageInfo& page : pages) {
    service_->GetLogger()->RecordActivity(
        "Response for page: " + page.url +
        "; status=" + std::to_string(static_cast<int>(page.status)));
  }
}

void PrefetchDispatcherImpl::FetchVisuals(
    std::unique_ptr<PrefetchDispatcher::IdsVector> remaining_ids,
    bool is_first_attempt) {
  if (remaining_ids->empty())
    return;

  int64_t offline_id = remaining_ids->back().first;
  ClientId client_id = std::move(remaining_ids->back().second);
  DCHECK(client_id.name_space == kSuggestedArticlesNamespace);
  remaining_ids->pop_back();

  service_->GetOfflinePageModel()->GetVisualsAvailability(
      offline_id,
      base::BindOnce(&PrefetchDispatcherImpl::VisualsAvailabilityChecked,
                     GetWeakPtr(), offline_id, std::move(client_id),
                     std::move(remaining_ids), is_first_attempt));
}

void PrefetchDispatcherImpl::VisualsAvailabilityChecked(
    int64_t offline_id,
    ClientId client_id,
    std::unique_ptr<PrefetchDispatcher::IdsVector> remaining_ids,
    bool is_first_attempt,
    VisualsAvailability availability) {
  if (availability.has_thumbnail && availability.has_favicon) {
    FetchVisuals(std::move(remaining_ids), is_first_attempt);
  } else {
    // Zine/Feed: thumbnail_fetcher is non-null only with Zine.
    ThumbnailFetcher* thumbnail_fetcher = service_->GetThumbnailFetcher();
    if (thumbnail_fetcher) {
      auto complete_callback = base::BindOnce(
          &PrefetchDispatcherImpl::ThumbnailFetchComplete, GetWeakPtr(),
          offline_id, std::move(remaining_ids), is_first_attempt, GURL());
      thumbnail_fetcher->FetchSuggestionImageData(client_id,
                                                  std::move(complete_callback));
    } else {
      task_queue_.AddTask(std::make_unique<GetVisualsInfoTask>(
          service_->GetPrefetchStore(), offline_id,
          base::BindOnce(&PrefetchDispatcherImpl::VisualsInfoReceived,
                         GetWeakPtr(), offline_id, std::move(remaining_ids),
                         is_first_attempt, availability)));
    }
  }
}

void PrefetchDispatcherImpl::VisualsInfoReceived(
    int64_t offline_id,
    std::unique_ptr<IdsVector> remaining_ids,
    bool is_first_attempt,
    VisualsAvailability availability,
    GetVisualsInfoTask::Result result) {
  GURL favicon_url = availability.has_favicon || result.favicon_url.is_empty()
                         ? GURL()
                         : result.favicon_url;

  if (!availability.has_thumbnail && !result.thumbnail_url.is_empty()) {
    FetchThumbnailByURL(
        base::BindOnce(&PrefetchDispatcherImpl::ThumbnailFetchComplete,
                       GetWeakPtr(), offline_id, std::move(remaining_ids),
                       is_first_attempt, favicon_url),
        service_->GetImageFetcher(), result.thumbnail_url);
  } else if (!favicon_url.is_empty()) {
    FetchFavicon(offline_id, std::move(remaining_ids), is_first_attempt,
                 favicon_url);
  } else {
    FetchVisuals(std::move(remaining_ids), is_first_attempt);
  }
}

void PrefetchDispatcherImpl::ThumbnailFetchComplete(
    int64_t offline_id,
    std::unique_ptr<IdsVector> remaining_ids,
    bool is_first_attempt,
    const GURL& favicon_url,
    const std::string& thumbnail) {
  if (!thumbnail.empty())
    service_->GetOfflinePageModel()->StoreThumbnail(offline_id, thumbnail);

  if (favicon_url.is_empty()) {
    FetchVisuals(std::move(remaining_ids), is_first_attempt);
  } else {
    FetchFavicon(offline_id, std::move(remaining_ids), is_first_attempt,
                 favicon_url);
  }
}

void PrefetchDispatcherImpl::FetchFavicon(
    int64_t offline_id,
    std::unique_ptr<IdsVector> remaining_ids,
    bool is_first_attempt,
    const GURL& favicon_url) {
  FetchFaviconByURL(
      base::BindOnce(&PrefetchDispatcherImpl::FaviconFetchComplete,
                     GetWeakPtr(), offline_id, std::move(remaining_ids),
                     is_first_attempt),
      service_->GetImageFetcher(), favicon_url);
}

void PrefetchDispatcherImpl::FaviconFetchComplete(
    int64_t offline_id,
    std::unique_ptr<IdsVector> remaining_ids,
    bool is_first_attempt,
    const std::string& favicon_data) {
  if (!favicon_data.empty()) {
    service_->GetOfflinePageModel()->StoreFavicon(offline_id, favicon_data);
  }

  FetchVisuals(std::move(remaining_ids), is_first_attempt);
}

}  // namespace offline_pages
