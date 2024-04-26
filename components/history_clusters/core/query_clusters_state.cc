// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/query_clusters_state.h"

#include <set>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/strings/grit/components_strings.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_service_task.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "url/gurl.h"
#include "ui/base/l10n/l10n_util.h"

namespace history_clusters {

namespace {

QueryClustersFilterParams GetFilterParamsFromFlags(const std::string& query) {
  QueryClustersFilterParams filter_params;
  // Journeys launched Synced visits by default in early 2024.
  filter_params.include_synced_visits = true;
  filter_params.group_clusters_by_content = false;

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
    history::HistoryService* history_service,
    const std::string& query,
    base::Time begin_time,
    bool recluster)
    : service_(service),
      history_service_(history_service),
      query_(query),
      begin_time_(begin_time),
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

  auto query_clusters_callback = &QueryClustersState::OnGotRawClusters;
  if (!query_.empty() && history_service_ &&
      base::FeatureList::IsEnabled(kSearchesFindUngroupedVisits)) {
    // If there's a search query, we also want to first tack on ungrouped
    // visits.
    query_clusters_callback = &QueryClustersState::GetUngroupedVisits;
  }

  base::TimeTicks query_start_time = base::TimeTicks::Now();
  query_clusters_task_ = service_->QueryClusters(
      ClusteringRequestSource::kJourneysPage, filter_params_, begin_time_,
      continuation_params_, recluster_,
      base::BindOnce(query_clusters_callback, weak_factory_.GetWeakPtr(),
                     query_start_time, std::move(callback)));
}

void QueryClustersState::GetUngroupedVisits(
    base::TimeTicks query_start_time,
    ResultCallback callback,
    std::vector<history::Cluster> clusters,
    QueryClustersContinuationParams new_continuation_params) {
  DCHECK(history_service_);

  // Find ungrouped visits within the timespan bounded by the batch of clusters
  // we have just received.
  history::QueryOptions options;
  options.end_time = continuation_params_.continuation_time;
  options.begin_time = new_continuation_params.continuation_time;

  // No need to use BrowsingHistoryService, because history is now fully synced.
  history_service_->GetAnnotatedVisits(
      options,
      /*compute_redirect_chain_start_properties=*/false,
      /*get_unclustered_visits_only=*/true,
      base::BindOnce(&QueryClustersState::OnGotUngroupedVisits,
                     weak_factory_.GetWeakPtr(), query_start_time,
                     std::move(callback), clusters, new_continuation_params),
      &history_task_tracker_);
}

void QueryClustersState::OnGotUngroupedVisits(
    base::TimeTicks query_start_time,
    ResultCallback callback,
    std::vector<history::Cluster> clusters,
    QueryClustersContinuationParams new_continuation_params,
    std::vector<history::AnnotatedVisit> ungrouped_visits) {
  // Load all the visits in `clusters` into `seen_visits` using similarity key.
  for (auto& cluster : clusters) {
    for (auto& visit : cluster.visits) {
      seen_visits_for_deduping_ungrouped_visits_.insert(SimilarVisit(visit));
    }
  }

  std::vector<history::ClusterVisit> unique_ungrouped_visits;
  for (auto& visit : ungrouped_visits) {
    history::ClusterVisit cluster_visit;
    cluster_visit.annotated_visit = visit;
    if (!visit.content_annotations.search_normalized_url.is_empty()) {
      cluster_visit.normalized_url =
          visit.content_annotations.search_normalized_url;
      cluster_visit.url_for_deduping = cluster_visit.normalized_url;
    } else {
      cluster_visit.normalized_url = visit.url_row.url();
      cluster_visit.url_for_deduping =
          ComputeURLForDeduping(cluster_visit.url_for_deduping);
    }

    auto [ignored_iterator, inserted] =
        seen_visits_for_deduping_ungrouped_visits_.insert(
            SimilarVisit(cluster_visit));
    if (inserted) {
      // Fill in these fields here to avoid doing so unless we're inserting this
      // visit.
      cluster_visit.url_for_display =
          ComputeURLForDisplay(cluster_visit.normalized_url);
      // Give every fake cluster visit a generic 1.0 score to ensure visibility.
      cluster_visit.score = 1.0;

      unique_ungrouped_visits.push_back(std::move(cluster_visit));
    }
  }

  if (!unique_ungrouped_visits.empty()) {
    history::Cluster ungrouped_cluster;
    ungrouped_cluster.visits = std::move(unique_ungrouped_visits);
    // Setting this for correctness, but should have no effect since the user
    // should be searching.
    ungrouped_cluster.should_show_on_prominent_ui_surfaces = false;
    ungrouped_cluster.label_source =
        history::Cluster::LabelSource::kUngroupedVisits;
    ungrouped_cluster.label = l10n_util::GetStringUTF16(
            IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_OTHER_MATCHING_VISITS);
    ungrouped_cluster.raw_label = ungrouped_cluster.label;
    clusters.push_back(std::move(ungrouped_cluster));
  }

  OnGotRawClusters(query_start_time, std::move(callback), std::move(clusters),
                   std::move(new_continuation_params));
}

void QueryClustersState::OnGotRawClusters(
    base::TimeTicks query_start_time,
    ResultCallback callback,
    std::vector<history::Cluster> clusters,
    QueryClustersContinuationParams new_continuation_params) {
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
          query_start_time, std::move(callback), new_continuation_params));
}

void QueryClustersState::OnGotClusters(
    base::ElapsedTimer post_processing_timer,
    size_t clusters_from_backend_count,
    base::TimeTicks query_start_time,
    ResultCallback callback,
    QueryClustersContinuationParams new_continuation_params,
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

  continuation_params_ = new_continuation_params;

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
  if (clusters.empty() && !new_continuation_params.exhausted_all_visits) {
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
                          !new_continuation_params.exhausted_all_visits,
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
