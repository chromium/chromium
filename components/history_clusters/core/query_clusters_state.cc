// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/query_clusters_state.h"

#include <set>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "url/gurl.h"

namespace history_clusters {

// Helper class that lives and is destroyed on the `sequenced_task_runner`,
// although it's created on the main thread. It allows us to store state that
// is only accessed on `sequenced_task_runner` that persists between batches.
class QueryClustersState::PostProcessor
    : public base::RefCountedDeleteOnSequence<PostProcessor> {
 public:
  PostProcessor(scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
                const std::string query)
      : base::RefCountedDeleteOnSequence<PostProcessor>(sequenced_task_runner),
        query_(query) {}
  PostProcessor(const PostProcessor& other) = delete;
  PostProcessor& operator=(const PostProcessor& other) = delete;

  std::vector<history::Cluster> PostProcess(
      std::vector<history::Cluster> clusters) {
    ApplySearchQuery(query_, &clusters);
    CullNonProminentOrDuplicateClusters(query_, &clusters,
                                        &seen_single_visit_cluster_urls_);
    return clusters;
  }

 private:
  friend class base::RefCountedDeleteOnSequence<PostProcessor>;
  friend class base::DeleteHelper<PostProcessor>;

  // Ref-counted object should only be deleted via ref-counting.
  ~PostProcessor() = default;

  const std::string query_;

  // URLs of single-visit non-prominent clusters we've already seen.
  std::set<GURL> seen_single_visit_cluster_urls_;
};

QueryClustersState::QueryClustersState(
    base::WeakPtr<HistoryClustersService> service,
    const std::string& query)
    : service_(service),
      query_(query),
      post_processing_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      post_processing_state_(
          base::MakeRefCounted<PostProcessor>(post_processing_task_runner_,
                                              query)) {}

QueryClustersState::~QueryClustersState() = default;

void QueryClustersState::LoadNextBatchOfClusters(ResultCallback callback) {
  if (!service_)
    return;

  base::TimeTicks query_start_time = base::TimeTicks::Now();
  base::Time end_time = continuation_end_time_.value_or(base::Time());
  service_->QueryClusters(ClusteringRequestSource::kJourneysPage,
                          /*begin_time=*/base::Time(), end_time,
                          base::BindOnce(&QueryClustersState::OnGotRawClusters,
                                         weak_factory_.GetWeakPtr(),
                                         query_start_time, std::move(callback)),
                          &task_tracker_);
}

void QueryClustersState::OnGotRawClusters(
    base::TimeTicks query_start_time,
    ResultCallback callback,
    std::vector<history::Cluster> clusters,
    base::Time continuation_end_time) const {
  // Post-process the clusters (expensive task) on an anonymous thread to
  // prevent janks.
  base::ElapsedTimer post_processing_timer;  // Create here to time the task.

  size_t clusters_from_backend_count = clusters.size();
  post_processing_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PostProcessor::PostProcess, post_processing_state_,
                     std::move(clusters)),
      base::BindOnce(
          &QueryClustersState::OnGotClusters, weak_factory_.GetWeakPtr(),
          std::move(post_processing_timer), clusters_from_backend_count,
          query_start_time, std::move(callback), continuation_end_time));
}

void QueryClustersState::OnGotClusters(base::ElapsedTimer post_processing_timer,
                                       size_t clusters_from_backend_count,
                                       base::TimeTicks query_start_time,
                                       ResultCallback callback,
                                       base::Time continuation_end_time,
                                       std::vector<history::Cluster> clusters) {
  base::UmaHistogramTimes("History.Clusters.ProcessClustersDuration",
                          post_processing_timer.Elapsed());

  if (clusters_from_backend_count > 0) {
    // Log the percentage of clusters that get filtered (e.g., 100 - % of
    // clusters that remain).
    base::UmaHistogramCounts100(
        "History.Clusters.PercentClustersFilteredByQuery",
        static_cast<int>(100 - (clusters.size() /
                                (1.0 * clusters_from_backend_count) * 100)));
  }

  continuation_end_time_.reset();
  if (!continuation_end_time.is_null())
    continuation_end_time_ = continuation_end_time;

  // In case no clusters came back, recursively ask for more here. We do this
  // to fulfill the mojom contract where we always return at least one cluster,
  // or we exhaust History. We don't do this in the service because of task
  // tracker lifetime difficulty. In practice, this only happens when the user
  // has a search query that doesn't match any of the clusters in the "page".
  // https://crbug.com/1263728
  //
  // This is distinct from the "tall monitor" case because the page may already
  // be full of clusters. In that case, the WebUI would not know to make another
  // request for clusters.
  if (clusters.empty() && continuation_end_time_.has_value()) {
    LoadNextBatchOfClusters(std::move(callback));
    return;
  }

  bool can_load_more = continuation_end_time_.has_value();
  std::move(callback).Run(query_, std::move(clusters), can_load_more,
                          is_continuation_);

  // Further responses should be consider continuations.
  is_continuation_ = true;

  // Log metrics after delivering the results to the page.
  base::TimeDelta service_latency = base::TimeTicks::Now() - query_start_time;
  base::UmaHistogramTimes("History.Clusters.ServiceLatency", service_latency);
}

}  // namespace history_clusters
