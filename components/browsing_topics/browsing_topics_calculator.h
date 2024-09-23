// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_CALCULATOR_H_
#define COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_CALCULATOR_H_

#include <map>
#include <set>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/browsing_topics/annotator.h"
#include "components/browsing_topics/common/common_types.h"
#include "components/browsing_topics/epoch_topics.h"
#include "components/history/core/browser/history_types.h"

namespace privacy_sandbox {
class PrivacySandboxSettings;
}  // namespace privacy_sandbox

namespace history {
class HistoryService;
}  // namespace history

namespace content {
class BrowsingTopicsSiteDataManager;
}  // namespace content

namespace browsing_topics {

// Responsible for doing a one-off browsing topics calculation. It will:
// 1) Check the user settings for calculation permissions.
// 2) Query the `BrowsingTopicsSiteDataManager` for the contexts where the
// Topics API was called on.
// 3) Query the `HistoryService` for the hosts of the pages the API was called
// on.
// 4) Query the `Annotator` with a set of hosts, to get the corresponding
// topics.
// 5) Derive `EpochTopics` (i.e. the top topics and the their observed-by
// contexts), and return it as the final result.
class BrowsingTopicsCalculator {
 public:
  using CalculateCompletedCallback = base::OnceCallback<void(EpochTopics)>;

  BrowsingTopicsCalculator(
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      Annotator* annotator,
      const base::circular_deque<EpochTopics>& epochs,
      bool is_manually_triggered,
      int previous_timeout_count,
      base::Time session_start_time,
      CalculateCompletedCallback callback);

  BrowsingTopicsCalculator(const BrowsingTopicsCalculator&) = delete;
  BrowsingTopicsCalculator& operator=(const BrowsingTopicsCalculator&) = delete;
  BrowsingTopicsCalculator(BrowsingTopicsCalculator&&) = delete;
  BrowsingTopicsCalculator& operator=(BrowsingTopicsCalculator&&) = delete;

  virtual ~BrowsingTopicsCalculator();

  bool is_manually_triggered() const { return is_manually_triggered_; }

  int previous_timeout_count() const { return previous_timeout_count_; }

 protected:
  // This method exists for the purposes of overriding in tests.
  virtual uint64_t GenerateRandUint64();
  virtual void CheckCanCalculate();

 private:
  enum class Progress {
    kStarted,
    kApiUsageRequested,
    kHistoryRequested,
    kModelRequested,
    kAnnotationRequested,
    kCompleted,
  };

  // Get the top `kBrowsingTopicsNumberOfTopTopicsPerEpoch` topics. If there
  // aren't enough topics, pad with random ones. Return the result topics, and
  // the starting index of the padded topics (or
  // `kBrowsingTopicsNumberOfTopTopicsPerEpoch` if there's no padded topics),
  // and the number of topics associated with `history_hosts_count`.
  void DeriveTopTopics(
      const std::map<HashedHost, size_t>& history_hosts_count,
      const std::map<HashedHost, std::set<Topic>>& host_topics_map,
      std::vector<Topic>& top_topics,
      size_t& padded_top_topics_start_index,
      size_t& history_topics_count);

  void OnGetRecentBrowsingTopicsApiUsagesCompleted(
      browsing_topics::ApiUsageContextQueryResult result);

  void OnGetRecentlyVisitedURLsCompleted(history::QueryResults results);

  void OnRequestModelCompleted(std::vector<std::string> raw_hosts);

  void OnGetTopicsForHostsCompleted(const std::vector<Annotation>& results);

  void OnCalculateCompleted(EpochTopics epoch_topics);

  void OnCalculationHanging();

  // Those pointers are safe to hold and use throughout the lifetime of
  // `BrowsingTopicsService`, which owns this object.
  raw_ptr<privacy_sandbox::PrivacySandboxSettings> privacy_sandbox_settings_;
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<content::BrowsingTopicsSiteDataManager> site_data_manager_;
  raw_ptr<Annotator> annotator_;

  CalculateCompletedCallback calculate_completed_callback_;

  // The calculation start time.
  base::Time calculation_time_;

  base::Time history_data_start_time_;
  base::Time api_usage_context_data_start_time_;

  Progress progress_ = Progress::kStarted;

  // The history hosts over
  // `kBrowsingTopicsNumberOfEpochsOfObservationDataToUseForFiltering` epochs,
  // and the calling context domains that used the Topics API in each main frame
  // host.
  std::map<HashedHost, std::vector<HashedDomain>> host_context_domains_map_;

  // The hashed history hosts and their count over the last epoch.
  std::map<HashedHost, size_t> history_hosts_count_;

  // Used for the async tasks querying the HistoryService.
  base::CancelableTaskTracker history_task_tracker_;

  // Whether this calculator was generated via the topics-internals page rather
  // than via a scheduled task.
  bool is_manually_triggered_;

  // The number of previous hanging calculations.
  int previous_timeout_count_;

  // The timeout timer for each async operation.
  base::OneShotTimer timeout_timer_;

  base::Time session_start_time_;

  base::WeakPtrFactory<BrowsingTopicsCalculator> weak_ptr_factory_{this};
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_CALCULATOR_H_
