// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_DISPATCHER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_DISPATCHER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"

namespace offline_pages {
struct ClientId;

// Testing version of the prefetch dispatcher.
class TestPrefetchDispatcher : public PrefetchDispatcher {
 public:
  TestPrefetchDispatcher();
  ~TestPrefetchDispatcher() override;

  // PrefetchDispatcher implementation.
  void AddCandidatePrefetchURLs(
      const std::string& name_space,
      const std::vector<PrefetchURL>& prefetch_urls) override;
  void NewSuggestionsAvailable(
      SuggestionsProvider* suggestions_provider) override;
  void RemoveSuggestion(const GURL& url) override;

  void RemoveAllUnprocessedPrefetchURLs(const std::string& name_space) override;
  void RemovePrefetchURLsByClientId(const ClientId& client_id) override;
  void BeginBackgroundTask(
      std::unique_ptr<PrefetchBackgroundTask> task) override;
  void StopBackgroundTask() override;
  void SetService(PrefetchService* service) override;
  void SchedulePipelineProcessing() override;
  void EnsureTaskScheduled() override;
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

  std::string latest_name_space;
  std::vector<PrefetchURL> latest_prefetch_urls;
  std::unique_ptr<ClientId> last_removed_client_id;
  std::vector<std::string> operation_list;
  std::vector<PrefetchDownloadResult> download_results;
  std::vector<std::pair<int64_t, ClientId>> item_downloaded_results;
  std::vector<std::pair<int64_t, bool>> import_results;
  std::unique_ptr<IdsVector> ids_from_generate_page_bundle_requested;

  int cleanup_downloads_count = 0;
  int new_suggestions_count = 0;
  int processing_schedule_count = 0;
  int remove_all_suggestions_count = 0;
  int remove_by_client_id_count = 0;
  int task_schedule_count = 0;
  int generate_page_bundle_requested = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_DISPATCHER_H_
