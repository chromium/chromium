// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/time/time.h"

namespace history_clusters {

namespace switches {

extern const char kShouldShowAllClustersOnProminentUiSurfaces[];

}  // namespace switches

// The default configuration. Always use |GetConfig()| to get the current
// configuration.
//
// Config has the same thread-safety as base::FeatureList. The first call to
// GetConfig() (which performs initialization) must be done single threaded on
// the main thread. After that, Config can be read from any thread.
struct Config {
  // The `kJourneys` feature and child params.

  // True if journeys feature is enabled as per field trial check. Does not
  // check for any user-specific conditions (such as locales).
  bool is_journeys_enabled_no_locale_check = false;

  // The max number of visits to use for each clustering iteration. This limits
  // the number of visits sent to the clustering backend per batch.
  int max_visits_to_cluster = 1000;

  // A soft cap on the number of keyword phrases to cache. 5000 should be more
  // than enough, as the 99.9th percentile of users has 2000. A few nuances:
  //  - We cache both entity keywords and URLs, each limited separately.
  //  - We have both a long and short duration cache, each limited separately.
  //  - We complete processing each cluster even if it means slightly going over
  //    this limit.
  //  - 0 and -1 are not interpreted as sentinel values. We always have a limit.
  size_t max_keyword_phrases = 5000;

  // If enabled, this is the min score that a visit needs to have to always be
  // shown above the fold regardless of the number of visits already shown.
  double min_score_to_always_show_above_the_fold = 0.5;

  // If enabled, this is the number of non-zero scored visits to always show
  // above the fold regardless of score. Note, this value includes the
  // "top visit". In the unlabeled "top visit" UI configuration, that means the
  // one "top visit" and three subordinate looking visits will be always shown.
  size_t num_visits_to_always_show_above_the_fold = 4;

  // If enabled, when there is a Journeys search query, the backend re-scores
  // visits within a cluster to account for whether or not that visit matches.
  bool rescore_visits_within_clusters_for_query = true;

  // If enabled, sorts clusters WITHIN a single batch from most search matches
  // to least search matches. The batches themselves will still be ordered
  // reverse chronologically, but the clusters within batches will be resorted.
  bool sort_clusters_within_batch_for_query = false;

  // The `kJourneysImages` feature and child params.

  // Whether to attempt to provide images for eligible Journeys.
  bool images = true;

  // Whether the image covers the whole icon container.
  bool images_cover = true;

  // Determines the minimum period to update clusters in minutes.
  int persist_clusters_in_history_db_period_minutes = 1;

  // Hard cap on max clusters to fetch after exhausting unclustered visits and
  // fetching persisted clusters for the get most recent flow. Doesn't affect
  // the update flow, which uses day boundaries as well as
  // `max_visits_to_cluster` to keep the number of clusters and visits
  // reasonable.
  size_t max_persisted_clusters_to_fetch = 100;

  // Like `max_persisted_clusters_to_fetch`, but an additional soft cap on max
  // visits in case there are a few very large clusters in the same batch.
  size_t max_persisted_cluster_visits_to_fetch_soft_cap = 1000;

  // The number of days of persisted clusters to recluster when updating
  // clusters. E.g., if set to 2, and clusters up to 1/10 have been persisted,
  // then the next request will include visits from clusters from 1/8 and 1/9,
  // and unclustered visits from 1/10.
  size_t persist_clusters_recluster_window_days = 0;

  // The `kOmniboxAction` feature and child params.

  // Enables the Journeys Omnibox Action chip. `kJourneys` must also be enabled
  // for this to take effect.
  bool omnibox_action = false;

  // If enabled, allows the Omnibox Action chip to appear when the suggestions
  // contain pedals. Does nothing if `omnibox_action` is disabled.
  bool omnibox_action_with_pedals = false;

  // If `omnibox_action_on_navigation_intents` is false, this threshold
  // helps determine when the user is intending to perform a navigation.
  int omnibox_action_navigation_intent_score_threshold = 1300;

  // If enabled, allows the Omnibox Action chip to appear when it's likely the
  // user is intending to perform a navigation. This does not affect which
  // suggestions are allowed to display the chip. Does nothing if
  // `omnibox_action` is disabled.
  bool omnibox_action_on_navigation_intents = false;

  // The `kOmniboxHistoryClusterProvider` feature and child params.

  // Enables `HistoryClusterProvider` to surface Journeys as a suggestion row
  // instead of an action chip. Enabling this won't actually disable
  // `omnibox_action_with_pedals`, but for user experiments, the intent is to
  // only have 1 enabled.
  bool omnibox_history_cluster_provider = false;

  // If `omnibox_history_cluster_provider` is enabled, hides its suggestions but
  // counterfactual logs when it has suggestions (though not necessarily shown
  // suggestions). Does nothing if `omnibox_history_cluster_provider` is
  // disabled.
  bool omnibox_history_cluster_provider_counterfactual = false;

  // The score the `HistoryClusterProvider` will assign to journey suggestions.
  // Meaningless if `omnibox_history_cluster_provider` is disabled. 900 seems to
  // work well in local tests. It's high enough to outscore search suggestions
  // and therefore not be crowded out, but low enough to only display when there
  // aren't too many strong navigation matches.
  int omnibox_history_cluster_provider_score = 900;

  // If enabled, will inherit the score from the matched search suggestion
  // minus 1. This will force the journey suggestion immediately after the
  // search suggestion, except if there's a tie with another suggestion, in
  // which case it's indeterminate which is ordered first. If enabled,
  // `omnibox_history_cluster_provider_score` becomes a no-op.
  bool omnibox_history_cluster_provider_inherit_search_match_score = false;

  // If `omnibox_history_cluster_provider_on_navigation_intents` is false, this
  // threshold helps determine when the user is intending to perform a
  // navigation. Meaningless if either `omnibox_history_cluster_provider` is
  // disabled or `omnibox_history_cluster_provider_on_navigation_intents` is
  // true
  int omnibox_history_cluster_provider_navigation_intent_score_threshold = 1300;

  // If enabled, allows the suggestion row to appear when it's likely the user
  // is intending to perform a navigation. Meaningless if
  // `omnibox_history_cluster_provider` is disabled.
  bool omnibox_history_cluster_provider_on_navigation_intents = false;

  // The `kOnDeviceClusteringKeywordFiltering` feature and child params.

  // If enabled, adds the keywords of aliases for detected entity names to a
  // cluster.
  bool keyword_filter_on_entity_aliases = false;

  // If greater than 0, the max number of aliases to include in keywords. If <=
  // 0, all aliases will be included.
  size_t max_entity_aliases_in_keywords = 0;

  // If enabled, adds the keywords of detected entities from noisy visits to a
  // cluster.
  bool keyword_filter_on_noisy_visits = false;

  // Maximum number of keywords to keep per cluster.
  size_t max_num_keywords_per_cluster = 20;

  // The `kOnDeviceClustering` feature and child params.

  // Returns the maximum duration between navigations that
  // a visit can be considered for the same cluster.
  base::TimeDelta cluster_navigation_time_cutoff = base::Minutes(60);

  // The minimum threshold for whether an entity is considered relevant to the
  // visit.
  int entity_relevance_threshold = 60;

  // Returns the threshold for which we should mark a cluster as being able to
  // show on prominent UI surfaces.
  float content_visibility_threshold = 0.7;

  // Returns the threshold used to determine if a cluster, and its visits, has
  // too high site engagement to be likely useful.
  float noisy_cluster_visits_engagement_threshold = 15.0;

  // Returns the number of visits considered interesting, or not noisy, required
  // to prevent the cluster from being filtered out (i.e., marked as not visible
  // on the zero state UI).
  size_t number_interesting_visits_filter_threshold = 1;

  // The `kUseEngagementScoreCache` feature and child params.

  // Whether to use a cache to store the site engagement scores per host. Used
  // in both the old (OnDeviceClusteringBackend) and new
  // (ContextClustererHistoryServiceObserver) clustering paths.
  bool use_engagement_score_cache = true;

  // The max number of hosts that should be stored in the engagement score
  // cache.
  int engagement_score_cache_size = 100;

  // The max time a host should be stored in the engagement score cache.
  base::TimeDelta engagement_score_cache_refresh_duration = base::Minutes(120);

  // The `kHistoryClustersVisitDeduping` feature and child params.

  // Use host instead of heavily-stripped URL as URL for deduping.
  bool use_host_for_visit_deduping = true;

  // The `kOnDeviceClusteringVisitRanking` feature and child params.

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

  // Returns the weight to use for visits with URL-keyed images when ranking
  // visits within a cluster. Will always be greater than or equal to 0.
  float has_url_keyed_image_ranking_weight = 1.5;

  // The `kHistoryClustersNavigationContextClustering` feature and child params.

  // Whether to use the new clustering path that does context clustering at
  // navigation and embellishes clusters for display at UI time.
  bool use_navigation_context_clusters = true;

  // The duration between context clustering clean up passes.
  base::TimeDelta context_clustering_clean_up_duration = base::Minutes(10);

  // The duration since the most recent visit for which a context cluster is
  // considered to be fully frozen and triggerability can be finalized.
  base::TimeDelta cluster_triggerability_cutoff_duration = base::Minutes(120);

  // WebUI features and params.

  // Whether new tab groups created by "Open all in new tab group" should be
  // named after the cluster title. If false, the new tab group is anonymous,
  // which is the pre-M115 behavior.
  bool named_new_tab_groups = true;

  // The `kJourneysZeroStateFiltering` feature and child params.

  bool apply_zero_state_filtering = true;

  // Lonely features without child params.

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

  // Whether to check if all visits for a host should be in resulting clusters.
  bool should_check_hosts_to_skip_clustering_for = false;

  // True if the task runner should use trait CONTINUE_ON_SHUTDOWN.
  bool use_continue_on_shutdown = true;

  // Whether to show all clusters on prominent UI surfaces unconditionally. This
  // should only be set to true via command line.
  bool should_show_all_clusters_unconditionally_on_prominent_ui_surfaces =
      false;

  // Whether keyword caches should be written to and read from prefs.
  bool persist_caches_to_prefs = true;

  // Order consistently with features.h.

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

// Gets the current configuration.
const Config& GetConfig();

// Overrides the config returned by |GetConfig()|.
void SetConfigForTesting(const Config& config);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_
