// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/time/time.h"

class PrefService;

namespace history_clusters {

namespace switches {

extern const char kShouldShowAllClustersOnProminentUiSurfaces[];

}  // namespace switches

class HistoryClustersService;

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

  // If enabled, hidden visits are dropped entirely, instead of being gated
  // behind a "Show More" UI control.
  bool drop_hidden_visits = true;

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

  // The `kJourneysImages` feature and child params.

  // Whether to attempt to provide images for eligible Journeys (so far just
  // a proof of concept implementation for Entities only).
  bool images = false;

  // The `kPersistedClusters` feature and child params.

  // If enabled, updating clusters will persist the results to the history DB
  // and accessing clusters will retrieve them from the history DB. If disabled,
  // updating clusters is a no-op and accessing clusters will generate and
  // return new clusters without persisting them.
  bool persist_clusters_in_history_db = false;

  // No effect if `persist_clusters_in_history_db` is disabled. Determines how
  // soon to update clusters after startup in minutes. E.g., by default, will
  // update clusters 5 minutes after startup.
  int persist_clusters_in_history_db_after_startup_delay_minutes = 5;

  // No effect if `persist_clusters_in_history_db` is disabled. Determines how
  // often to update clusters in minutes. E.g., by default, will update clusters
  // every hour.
  int persist_clusters_in_history_db_period_minutes = 60;

  // No effect if `persist_clusters_in_history_db` is disabled. If disabled,
  // persistence occurs on a timer (see the above 2 params). If enabled, will
  // instead occur on query like refreshing the keyword cache does. This may
  // help bound the number of persistence requests. If enabled, will continue to
  // also be capped to at most 1 request per
  // `persist_clusters_in_history_db_period_minutes`, but
  // `persist_clusters_in_history_db_after_startup_delay_minutes` will be
  // unused.
  bool persist_on_query = false;

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
  size_t persist_clusters_recluster_window_days = 2;

  // The `kOmniboxAction` feature and child params.

  // Enables the Journeys Omnibox Action chip. `kJourneys` must also be enabled
  // for this to take effect.
  bool omnibox_action = false;

  // If enabled, allows the Omnibox Action chip to also appear on URLs. This
  // does nothing if `omnibox_action` is disabled. Note, that if you turn this
  // flag to true, you almost certainly will want to set
  // `omnibox_action_on_navigation_intents` to true as well, as otherwise your
  // desired action chips on URLs will almost certainly all be suppressed.
  bool omnibox_action_on_urls = false;

  // If enabled, allows the Omnibox Action chip to appear on URLs from noisy
  // visits. This does nothing if `omnibox_action_on_urls` is disabled.
  bool omnibox_action_on_noisy_urls = true;

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

  // If enabled, allowed the action chip to appear on search entity suggestions.
  // TODO(crbug.com/1394812): Clean this flag up beyond M110.
  bool omnibox_action_on_entities = true;

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
  bool omnibox_history_cluster_provider_shortcuts = false;

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

  // Returns the threshold used to determine if a cluster, and its visits, has
  // too high site engagement to be likely useful.
  float noisy_cluster_visits_engagement_threshold = 15.0;

  // Returns the number of visits considered interesting, or not noisy, required
  // to prevent the cluster from being filtered out (i.e., marked as not visible
  // on the zero state UI).
  size_t number_interesting_visits_filter_threshold = 1;

  // The `kJourneysCategoryFiltering` feature and child params.

  // Whether to determine whether to show/hide clusters on prominent UI surfaces
  // based on categories annotated for a visit.
  bool should_use_categories_to_filter_on_prominent_ui_surfaces = false;

  // The category IDs used for filtering. These should represent categories that
  // are repesentatitive of Journeys that we think the user is likely to want to
  // re-engage with.
  base::flat_set<std::string> categories_for_filtering;

  // The `kOnDeviceClusteringContentClustering` feature and child params.

  // Returns whether content clustering is enabled and
  // should be performed by the clustering backend.
  bool content_clustering_enabled = false;

  // Returns the weight that should be placed on entity similarity for
  // determining if two clusters are similar enough to be combined into one.
  float content_clustering_entity_similarity_weight = 1.0;

  // Returns the similarity threshold, between 0 and 1, used to determine if
  // two clusters are similar enough to be combined into
  // a single cluster.
  float content_clustering_similarity_threshold = 0.2;

  // Returns the threshold for which we should mark a cluster as being able to
  // show on prominent UI surfaces.
  float content_visibility_threshold = 0.7;

  // Returns true if content clustering should use the intersection similarity
  // score.
  bool content_cluster_on_intersection_similarity = false;

  // Returns the threshold, in terms of the number of overlapping keywords, to
  // use when clustering based on intersection score.
  int cluster_interaction_threshold = 2;

  // Returns true if content clustering should use the cosine similarity
  // algorithm.
  bool content_cluster_using_cosine_similarity = false;

  // Returns whether we should exclude entities that do not have associated
  // collections from content clustering.
  bool exclude_entities_that_have_no_collections_from_content_clustering =
      false;

  // The set of collections to block from being content clustered.
  base::flat_set<std::string> collections_to_block_from_content_clustering;

  // The `kUseEngagementScoreCache` feature and child params.

  // The max number of hosts that should be stored in the engagement score
  // cache.
  int engagement_score_cache_size = 100;

  // The max time a host should be stored in the engagement score cache.
  base::TimeDelta engagement_score_cache_refresh_duration = base::Minutes(120);

  // The `kHistoryClustersVisitDeduping` feature and child params.

  // Use host instead of heavily-stripped URL as URL for deduping.
  bool use_host_for_visit_deduping = false;

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

  // The `kHistoryClustersNavigationContextClustering` feature and child params.

  // The duration between context clustering clean up passes.
  base::TimeDelta context_clustering_clean_up_duration = base::Minutes(10);

  // Whether to persist the context clusters as the visits are coming in at
  // navigation time.
  bool persist_context_clusters_at_navigation = false;

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

  // Order consistently with features.h.

  Config();
  Config(const Config& other);
  ~Config();
};

// Returns the set of collections that should not be included for content
// clustering.
base::flat_set<std::string> JourneysCollectionContentClusteringBlocklist();

// Returns the set of categories that should be used to filter for whether a
// user is likely to re-engage with a cluster.
base::flat_set<std::string> JourneysCategoryFilteringAllowlist();

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

// Checks some prerequisites for history cluster omnibox suggestions and
// actions.
bool IsJourneysEnabledInOmnibox(HistoryClustersService* service,
                                PrefService* prefs);

// Gets the current configuration.
const Config& GetConfig();

// Overrides the config returned by |GetConfig()|.
void SetConfigForTesting(const Config& config);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_
