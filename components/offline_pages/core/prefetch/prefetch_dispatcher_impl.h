// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/server_forbidden_check_request.h"
#include "components/offline_pages/core/prefetch/suggestions_provider.h"
#include "components/offline_pages/core/prefetch/tasks/get_visuals_info_task.h"
#include "components/offline_pages/task/task_queue.h"
#include "components/version_info/channel.h"
#include "net/url_request/url_request_context_getter.h"

class PrefService;

namespace offline_pages {
class PrefetchService;
struct PrefetchSuggestion;

class PrefetchDispatcherImpl : public PrefetchDispatcher,
                               public TaskQueue::Delegate {
 public:
  explicit PrefetchDispatcherImpl(PrefService* pref_service);
  ~PrefetchDispatcherImpl() override;

  // PrefetchDispatcher implementation:
  void SetService(PrefetchService* service) override;
  void EnsureTaskScheduled() override;
  void SchedulePipelineProcessing() override;
  void AddCandidatePrefetchURLs(
      const std::string& name_space,
      const std::vector<PrefetchURL>& prefetch_urls) override;
  void NewSuggestionsAvailable(
      SuggestionsProvider* suggestions_provider) override;
  void RemoveSuggestion(const GURL& url) override;
  void RemoveAllUnprocessedPrefetchURLs(const std::string& name_space) override;
  void RemovePrefetchURLsByClientId(const ClientId& client_id) override;
  void BeginBackgroundTask(
      std::unique_ptr<PrefetchBackgroundTask> background_task) override;
  void StopBackgroundTask() override;
  void GCMOperationCompletedMessageReceived(
      const std::string& operation_name) override;
  void CleanupDownloads(
      const std::set<std::string>& outstanding_download_ids,
      const std::map<std::string, std::pair<base::FilePath, int64_t>>&
          success_downloads) override;
  void GeneratePageBundleRequested(std::unique_ptr<IdsVector> ids) override;
  void DownloadCompleted(
      const PrefetchDownloadResult& download_result) override;
  void ItemDownloaded(int64_t offline_id, const ClientId& client_id) override;
  void ArchiveImported(int64_t offline_id, bool success) override;

  // TaskQueue::Delegate implementation:
  void OnTaskQueueIsIdle() override;

 private:
  friend class PrefetchDispatcherTest;

  base::WeakPtr<PrefetchDispatcherImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void DisposeTask();

  // Callbacks for network requests.
  void DidGenerateBundleOrGetOperationRequest(
      const std::string& request_name_for_logging,
      PrefetchRequestStatus status,
      const std::string& operation_name,
      const std::vector<RenderPageInfo>& pages);
  void LogRequestResult(const std::string& request_name_for_logging,
                        PrefetchRequestStatus status,
                        const std::string& operation_name,
                        const std::vector<RenderPageInfo>& pages);

  // Adds the Reconcile tasks to the TaskQueue. These look for error/stuck
  // processing conditions that happen as result of Chrome being evicted
  // or network failures of certain kind. They are run on periodic wakeup
  // (BeginBackgroundTask()). See PrefetchDispatcher interface
  // declaration for Reconcile tasks definition.
  void QueueReconcileTasks();
  // Adds the Action tasks to the queue. See PrefetchDispatcher interface
  // declaration for Action tasks definition.
  // Action tasks can be added to the queue either in response to periodic
  // wakeup (when BeginBackgroundTask() is called) or any time TaskQueue
  // becomes idle and any task called SchedulePipelineProcessing() before.
  void QueueActionTasks();
  // Adds a list of PrefetchSuggestions to the queue of suggestions to be
  // prefetched.
  void AddSuggestions(std::vector<PrefetchSuggestion> suggestions);

  // The methods below control the  downloading of visuals for the provided
  // prefetch items IDs. They are called multiple times for the same article,
  // when they reach different points in the pipeline to increase the likeliness
  // of the thumbnail to be available. The existence of the thumbnail is
  // verified to avoid re-downloads.
  // Also, even though unlikely, concurrent calls to these methods are
  // supported. They will generate simultaneous download attempts but there will
  // be no impact in the consistency of stored data.
  // TODO(carlosk): This logic has become complex and holds too much state
  // throughout the calls. It should be moved into a separate class (possibly
  // internal to the implementation) to make it easier to maintain and
  // understand.
  void FetchVisuals(std::unique_ptr<IdsVector> remaining_ids,
                    bool is_first_attempt);
  void VisualsAvailabilityChecked(int64_t offline_id,
                                  ClientId client_id,
                                  std::unique_ptr<IdsVector> remaining_ids,
                                  bool is_first_attempt,
                                  VisualsAvailability availability);
  void VisualsInfoReceived(int64_t offline_id,
                           std::unique_ptr<IdsVector> remaining_ids,
                           bool is_first_attempt,
                           VisualsAvailability availability,
                           GetVisualsInfoTask::Result result);
  void ThumbnailFetchComplete(int64_t offline_id,
                              std::unique_ptr<IdsVector> remaining_ids,
                              bool is_first_attempt,
                              const GURL& favicon_url,
                              const std::string& thumbnail);
  void FetchFavicon(int64_t offline_id,
                    std::unique_ptr<IdsVector> remaining_ids,
                    bool is_first_attempt,
                    const GURL& favicon_url);
  void FaviconFetchComplete(int64_t offline_id,
                            std::unique_ptr<IdsVector> remaining_ids,
                            bool is_first_attempt,
                            const std::string& favicon_data);

  PrefService* pref_service_;
  PrefetchService* service_;
  TaskQueue task_queue_;
  bool needs_pipeline_processing_ = false;
  bool suspended_ = false;
  std::unique_ptr<PrefetchBackgroundTask> background_task_;
  base::WeakPtrFactory<PrefetchDispatcherImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrefetchDispatcherImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_IMPL_H_
