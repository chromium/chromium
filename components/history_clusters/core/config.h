// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_

#include <string>

#include "base/time/time.h"

namespace history_clusters {

// The default configuration. Always use |GetConfig()| to get the current
// configuration.
struct Config {
  // True if journeys feature is enabled as per field trial check. Does not
  // check for any user-specific conditions (such as locales).
  bool is_journeys_enabled_no_locale_check = false;

  // The max number of visits to use for each clustering iteration. This limits
  // the number of visits sent to the clustering backend per batch.
  int max_visits_to_cluster = 1000;

  // The recency threshold controlling which visits will be clustered. This
  // isn't the only factor; i.e. visits older than `MaxDaysToCluster()` may
  // still be clustered. Only applies when using persisted visit context
  // annotations; i.e. `kPersistContextAnnotationsInHistoryDb` is true.
  int max_days_to_cluster = 9;

  // A soft cap on the number of keyword phrases to cache. If 0, there is no
  // limit.
  // 20k should be more than enough for most cases and should avoid consuming
  // large amounts of memory in extreme cases.
  int max_keyword_phrases = 20000;

  // If enabled, updating clusters will persist the results to the history DB
  // and accessing clusters will retrieve them from the history DB. If disabled,
  // updating clusters is a no-op and accessing clusters will generate and
  // return new clusters without persisting them.
  bool persist_clusters_in_history_db = false;

  // Enables the on-device clustering backend. Enabled by default, since this is
  // the production mode of the whole feature. The backend is only in official
  // builds, so it won't work in unofficial builds.
  // bool use_on_device_clustering_backend;

  // If enabled, this is the min score that a visit needs to have to always be
  // shown above the fold regardless of the number of visits already shown.
  double min_score_to_always_show_above_the_fold = 0.5;

  // If enabled, this is the number of non-zero scored visits to always show
  // above the fold regardless of score.
  int num_visits_to_always_show_above_the_fold = 3;

  // If enabled, when there is a Journeys search query, the backend re-scores
  // visits within a cluster to account for whether or not that visit matches.
  bool rescore_visits_within_clusters_for_query = true;

  // If enabled, sorts clusters WITHIN a single batch from most search matches
  // to least search matches. The batches themselves will still be ordered
  // reverse chronologically, but the clusters within batches will be resorted.
  bool sort_clusters_within_batch_for_query = false;

  // If enabled, changes the History Clusters omnibox action text to be:
  // "Resume your research" instead of "Resume your journey".
  bool alternate_omnibox_action_text = true;

  // Enables the Journeys Omnibox Action chip. `kJourneys` must also be enabled
  // for this to take effect.
  bool omnibox_action = false;

  // Enables debug info in non-user-visible surfaces, like Chrome Inspector.
  // Does nothing if `kJourneys` is disabled.
  bool non_user_visible_debug = false;

  // Enables debug info in user-visible surfaces, like the actual WebUI page.
  // Does nothing if `kJourneys` is disabled.
  bool user_visible_debug = false;

  // Enables persisting context annotations in the History DB. They are always
  // calculated anyways. This just enables storing them. This is expected to be
  // enabled for all users shortly. This just provides a killswitch.
  // This flag is to enable us to turn on persisting context annotations WITHOUT
  // exposing the Memories UI in general. If EITHER this flag or `kJourneys` is
  // enabled, users will have context annotations persisted into their History
  // DB.
  bool persist_context_annotations_in_history_db = false;

  // Enables the history clusters internals page.
  bool history_clusters_internals_page = false;

  // Returns the maximum duration between navigations that
  // a visit can be considered for the same cluster.
  base::TimeDelta cluster_navigation_time_cutoff = base::Minutes(60);

  // Returns whether content clustering is enabled and
  // should be performed by the clustering backend.
  bool content_clustering_enabled = true;

  // Returns the weight that should be placed on entity similarity for
  // determining if two clusters are similar enough to be combined into one.
  float content_clustering_entity_similarity_weight = 1.0;

  // Returns the weight that should be placed on category similarity for
  // determining if two clusters are similar enough to be combined into one.
  float content_clustering_category_similarity_weight = 1.0;

  // Returns the similarity threshold, between 0 and 1, used to determine if
  // two clusters are similar enough to be combined into
  // a single cluster.
  float content_clustering_similarity_threshold = 0.2;

  // Returns the threshold for which we should mark a cluster as being able to
  // show on prominent UI surfaces.
  float content_visibility_threshold = 0.7;

  // Whether to hide single-visit clusters on prominent UI surfaces.
  bool should_hide_single_visit_clusters_on_prominent_ui_surfaces = true;

  // Whether to collapse visits within a cluster that will show on the UI in the
  // same way.
  bool should_dedupe_similar_visits = true;

  // Whether to filter clusters that are noisy from the UI. This will
  // heuristically remove clusters that are unlikely to be "interesting".
  bool should_filter_noisy_clusters = true;

  // Returns the threshold used to determine if a cluster, and its visits, has
  // too high site engagement to be likely useful.
  float noisy_cluster_visits_engagement_threshold = 15.0;

  // Returns the number of visits considered interesting, or not noisy, required
  // to prevent the cluster from being filtered out (i.e., marked as not visible
  // on the zero state UI).
  size_t number_interesting_visits_filter_threshold = 1;

  // Returns the weight to use for the visit duration when ranking visits within
  // a cluster. Will always be greater than or equal to 0.
  float visit_duration_ranking_weight = 1.0;

  // Returns the weight to use for the foreground duration when ranking visits
  // within a cluster. Will always be greater than or equal to 0.
  float foreground_duration_ranking_weight = 1.5;

  // Returns the weight to use for bookmarked visits when ranking visits within
  // a cluster. Will always be greater than or equal to 0.
  float bookmark_ranking_weight = 1.0;

  // Returns the weight to use for visits that are search results pages ranking
  // visits within a cluster. Will always be greater than or equal to 0.
  float search_results_page_ranking_weight = 2.0;

  // Returns the weight to use for visits that have page titles ranking visits
  // within a cluster. Will always be greater than or equal to 0.
  float has_page_title_ranking_weight = 2.0;

  // Returns true if content clustering should use the intersection similarity
  // score. Note, if this is used, the threshold used for clustering by content
  // score should be < .5 (see ContentClusteringSimilarityThreshold above) or
  // the weightings between entity and category content similarity scores should
  // be adjusted.
  bool content_cluster_on_intersection_similarity = true;

  // Returns the threshold, in terms of the number of overlapping keywords, to
  // use when clustering based on intersection score.
  int cluster_interaction_threshold = 2;

  // Whether to include category names in the keywords for a cluster.
  bool should_include_categories_in_keywords = true;

  // Whether to exclude keywords from visits that may be considered "noisy" to
  // the user (i.e. highly engaged, non-SRP).
  bool should_exclude_keywords_from_noisy_visits = false;

  // Returns the default batch size for annotating visits when clustering.
  size_t clustering_tasks_batch_size = 250;

  // Whether to split the clusters when a search visit is encountered.
  bool split_clusters_at_search_visits = true;

  // Whether to assign labels to clusters. If the label exists, it will be shown
  // in the UI. If the label doesn't exist, the UI will emphasize the top visit.
  bool should_label_clusters = false;

  Config();
  Config(const Config& other);
  ~Config();
};

// Returns true if |application_locale| is supported by Journeys.
// This is a costly check: Should be called only if
// |is_journeys_enabled_no_locale_check| is true, and the result should be
// cached.
bool IsApplicationLocaleSupportedByJourneys(
    const std::string& application_locale);

// Gets the current configuration. OverrideWithFinch() must have been called
// before GetConfig() is called.
const Config& GetConfig();

// Overrides the config returned by |GetConfig()|.
void SetConfigForTesting(const Config& config);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_
