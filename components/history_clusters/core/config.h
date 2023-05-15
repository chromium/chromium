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

  // The `kJourneysLabels` feature and child params.

  // Whether to assign labels to clusters from the hostnames of the cluster.
  // Does nothing if `should_label_clusters` is false. Note that since every
  // cluster has a hostname, this flag in conjunction with
  // `should_label_clusters` will give every cluster a label.
  bool labels_from_hostnames = true;

  // Whether to assign labels to clusters from the Entities of the cluster.
  // Does nothing if `should_label_clusters` is false.
  bool labels_from_entities = false;

  // Whether to assign labels to clusters from the entities associated with
  // search visits within a cluster if there are multiple search visits for the
  // cluster.
  bool labels_from_search_visit_entities = false;

  // The `kJourneysImages` feature and child params.

  // Whether to attempt to provide images for eligible Journeys (so far just
  // a proof of concept implementation for Entities only).
  bool images = false;

  // Whether the image covers the whole icon container.
  bool images_cover = true;

  // The `kPersistedClusters` feature and child params.

  // If enabled, updating clusters will persist the results to the history DB
  // and accessing clusters will retrieve them from the history DB. If disabled,
  // updating clusters is a no-op and accessing clusters will generate and
  // return new clusters without persisting them.
  bool persist_clusters_in_history_db = true;

  // No effect if `persist_clusters_in_history_db` is disabled. Determines how
  // soon to update clusters after startup in minutes. E.g., by default, will
  // update clusters 5 minutes after startup.
  int persist_clusters_in_history_db_after_startup_delay_minutes = 1;

  // No effect if `persist_clusters_in_history_db` is disabled. Determines how
  // often to update clusters in minutes. E.g., by default, will update clusters
  // every 1 hour.
  int persist_clusters_in_history_db_period_minutes = 1;

  // No effect if `persist_clusters_in_history_db` is disabled. If disabled,
  // persistence occurs on a timer (see the above 2 params). If enabled, will
  // instead occur on query like refreshing the keyword cache does. This may
  // help bound the number of persistence requests. If enabled, will continue to
  // also be capped to at most 1 request per
  // `persist_clusters_in_history_db_period_minutes`, but
  // `persist_clusters_in_history_db_after_startup_delay_minutes` will be
  // unused.
  bool persist_on_query = true;

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

  // If enabled, will inherit the score from the matched search suggestion. This
  // tries to emulate the ranking of chips, though remains slightly more
  // conservative in that chips will be shown if the match query is at least the
  // 8th top scored suggestion, while rows will be shown if the matched query is
  // at least the 7th top scored suggestion. If enabled,
  // `omnibox_history_cluster_provider_score` becomes a no-op.
  bool omnibox_history_cluster_provider_inherit_search_match_score = false;

  // If enabled, ranks the suggestion row below the default suggestion, but
  // above the searches. Though whether it appears or not will depend on scores.
  // Otherwise, ranks the suggestion among the search group; the exact position
  // will depend on scores.
  bool omnibox_history_cluster_provider_rank_above_searches = false;

  // Whether Journey suggestions from the `HistoryClusterProvider` can be
  // surfaced from the shortcuts' provider. They will be scored according to the
  // shortcuts' provider's scoring, which is more aggressive than the default
  // 900 score the `HistoryClusterProvider` assigns. Journey suggestions will
  // still be limited to 1, and will still be locked to the last suggestion
  // slot. More aggressive scoring won't affect ranking, but visibility. If
  // disabled, journey suggestions will still be added to the table, but
  // filtered out when retrieving suggesting; this is so that users in an
  // experiment group with `omnibox_history_cluster_provider_shortcuts` enabled
  // don't have lingering effects when they leave the group. Meaningless if
  // `omnibox_history_cluster_provider` is disabled.
  bool omnibox_history_cluster_provider_shortcuts = true;

  // Whether journey suggestions from the `ShortcutsProvider` can be default.
  // Journey suggestions from the `HistoryClusterProvider` can never be default.
  bool omnibox_history_cluster_provider_allow_default = false;

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

  // The `kOnDeviceClusteringContentClustering` feature and child params.

  // Returns whether content clustering is enabled and
  // should be performed by the clustering backend.
  bool content_clustering_enabled = false;

  // Returns whether content clustering should only be done across clusters that
  // contain a search.
  bool content_clustering_search_visits_only = false;

  // Returns the similarity threshold, between 0 and 1, used to determine if
  // two clusters are similar enough to be combined into
  // a single cluster.
  float content_clustering_similarity_threshold = 0.2;

  // Returns whether we should exclude entities that do not have associated
  // collections from content clustering.
  bool exclude_entities_that_have_no_collections_from_content_clustering = true;

  // The set of collections to block from being content clustered.
  base::flat_set<std::string> collections_to_block_from_content_clustering = {
      "/collection/it_glossary", "/collection/periodicals",
      "/collection/software", "/collection/websites"};

  // Whether to merge similar clusters using pairwise merge.
  bool use_pairwise_merge = false;

  // The maximum number of iterations to run for the convergence of pairwise
  // merging of similar clusters.
  int max_pairwise_merge_iterations = 40;

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

  // Whether show either the hide visits thumbs-down or menu item on individual
  // visits of persisted clusters. Which is shown depends on `hide_visits_icon`.
  bool hide_visits = false;

  // Whether to the icon or menu item.
  bool hide_visits_icon = true;

  // Whether new tab groups created by "Open all in new tab group" should be
  // named after the cluster title. If false, the new tab group is anonymous,
  // which is the pre-M115 behavior.
  bool named_new_tab_groups = true;

  // The `kUseUrlForDisplayCache` feature and child params.

  // Whether to use a cache to store the site engagement scores per host. Used
  // in both the old (OnDeviceClusteringBackend) and new
  // (ContextClustererHistoryServiceObserver) clustering paths.
  bool use_url_for_display_cache = false;

  // The max number of URLs that should be stored in the URL for display cache.
  int url_for_display_cache_size = 100;

  // The `kJourneysZeroStateFiltering` feature and child params.

  bool apply_zero_state_filtering = false;

  // The `kNtpChromeCartInHistoryClusterModule` child params.

  // Whether to use the NTP-specific algorithms and signals for determining
  // intracluster ranking.
  bool use_ntp_specific_intracluster_ranking = false;

  // Returns the weight to use for the visit duration when ranking visits within
  // a cluster. Will always be greater than or equal to 0 specifically on the
  // NTP surface when `use_ntp_specific_intracluster_ranking is true`.
  float ntp_visit_duration_ranking_weight = 1.0;

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

  // Whether to include synced visits in clusters.
  bool include_synced_visits = false;

  // Whether keyword caches should be written to and read from prefs.
  bool persist_caches_to_prefs = false;

  // Order consistently with features.h.

  Config();
  Config(const Config& other);
  ~Config();
};

// Returns the set of collections that should not be included for content
// clustering. If the experiment string is empty or malformed, `default_value`
// will be used.
base::flat_set<std::string> JourneysCollectionContentClusteringBlocklist(
    const base::flat_set<std::string>& default_value);

// Returns the set of mids that should be blocked from being used by the
// clustering backend, particularly for potential keywords used for omnibox
// triggering.
base::flat_set<std::string> JourneysMidBlocklist();

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
