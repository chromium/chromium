// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/ui/query_clusters_state.h"

#include <set>
#include <string>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_service_task_get_most_recent_clusters.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

QueryClustersFilterParams GetFilterParamsFromFlags(const std::string& query) {
  QueryClustersFilterParams filter_params;
  filter_params.include_synced_visits = GetConfig().include_synced_visits;
  filter_params.group_clusters_by_content =
      GetConfig().content_clustering_enabled;

  // If there is a query, we do not want to apply any filtering.
  if (!query.empty()) {
    return filter_params;
  }

  // Only set special filter params if the zero state filtering flag is applied.
  if (!ShouldUseNavigationContextClustersFromPersistence() ||
      !GetConfig().apply_zero_state_filtering) {
    return filter_params;
  }

  filter_params.is_search_initiated = true;
  filter_params.is_shown_on_prominent_ui_surfaces = true;
  // TODO(b/277528165): Apply category filtering only for eligible users.
  return filter_params;
}

}  // namespace

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
    ApplySearchQuery(query_, clusters);
    CullNonProminentOrDuplicateClusters(query_, clusters,
                                        &seen_single_visit_cluster_urls_);

    // Only sort after we figured out what we are showing.
    SortClusters(&clusters);

    // We have to do this AFTER applying the search query, because applying the
    // search query re-scores matching visits to promote them above non-matching
    // visits. Show 1-visit clusters only in query mode.
    CullVisitsThatShouldBeHidden(clusters,
                                 /*is_zero_query_state=*/query_.empty());
    // Do this AFTER we cull the low scoring visits, so those visits don't get
    // their related searches coalesced onto the cluster level.
    CoalesceRelatedSearches(clusters);
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
    const std::string& query,
    bool recluster)
    : service_(service),
      query_(query),
      filter_params_(GetFilterParamsFromFlags(query)),
      recluster_(recluster),
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
  query_clusters_task_ = service_->QueryClusters(
      ClusteringRequestSource::kJourneysPage, filter_params_,
      /*begin_time=*/base::Time(), continuation_params_, recluster_,
      base::BindOnce(&QueryClustersState::OnGotRawClusters,
                     weak_factory_.GetWeakPtr(), query_start_time,
                     std::move(callback)));
}

void QueryClustersState::OnGotRawClusters(
    base::TimeTicks query_start_time,
    ResultCallback callback,
    std::vector<history::Cluster> clusters,
    QueryClustersContinuationParams continuation_params) const {
  // Post-process the clusters (expensive task) on an anonymous thread to
  // prevent janks.
  base::ElapsedTimer post_processing_timer;  // Create here to time the task.

  size_t clusters_from_backend_count = clusters.size();
  post_processing_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PostProcessor::PostProcess, post_processing_state_,
                     std::move(clusters)),
      base::BindOnce(
          &QueryClustersState::OnGotClusters, weak_factory_.GetMutableWeakPtr(),
          std::move(post_processing_timer), clusters_from_backend_count,
          query_start_time, std::move(callback), continuation_params));
}

void QueryClustersState::OnGotClusters(
    base::ElapsedTimer post_processing_timer,
    size_t clusters_from_backend_count,
    base::TimeTicks query_start_time,
    ResultCallback callback,
    QueryClustersContinuationParams continuation_params,
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

  continuation_params_ = continuation_params;

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
  if (clusters.empty() && !continuation_params.exhausted_all_visits) {
    LoadNextBatchOfClusters(std::move(callback));
    return;
  }

  // This feels like it belongs in `PostProcessor`, but this operates on the
  // main thread, because the data needs to live on the main thread. Doing it
  // on the task runner requires making heap copies, which probably costs more
  // than just doing this simple computation on the main thread.
  UpdateUniqueRawLabels(clusters);

  size_t clusters_size = clusters.size();
  bool is_continuation = number_clusters_sent_to_page_ > 0;
  std::move(callback).Run(query_, std::move(clusters),
                          !continuation_params.exhausted_all_visits,
                          is_continuation);

  number_clusters_sent_to_page_ += clusters_size;

  // Log metrics after delivering the results to the page.
  base::TimeDelta service_latency = base::TimeTicks::Now() - query_start_time;
  base::UmaHistogramTimes("History.Clusters.ServiceLatency", service_latency);
}

void QueryClustersState::UpdateUniqueRawLabels(
    const std::vector<history::Cluster>& clusters) {
  // Skip this computation when there's a search query.
  if (!query_.empty())
    return;

  for (const auto& cluster : clusters) {
    if (!cluster.raw_label)
      return;

    const auto& raw_label_value = cluster.raw_label.value();
    // Warning: N^2 algorithm below. If this ends up scaling poorly, it can be
    // optimized by adding a map that tracks which labels have been seen
    // already.
    auto it = base::ranges::find(raw_label_counts_so_far_, raw_label_value,
                                 &LabelCount::first);
    if (it == raw_label_counts_so_far_.end()) {
      it = raw_label_counts_so_far_.insert(it,
                                           std::make_pair(raw_label_value, 0));
    }
    it->second++;
  }
}

}  // namespace history_clusters
