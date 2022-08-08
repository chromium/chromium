// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/time/time.h"

class PrefService;

namespace history_clusters {

class HistoryClustersService;

// The default configuration. Always use |GetConfig()| to get the current
// configuration.
//
// Config has the same thread-safety as base::FeatureList. The first call to
// GetConfig() (which performs initialization) must be done single threaded on
// the main thread. After that, Config can be read from any thread.
struct Config {
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

  // Enables the on-device clustering backend. Enabled by default, since this is
  // the production mode of the whole feature. The backend is only in official
  // builds, so it won't work in unofficial builds.
  // bool use_on_device_clustering_backend;

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

  // If enabled, allows the Omnibox Action chip to appear when it's likely the
  // user is intending to perform a navigation. This does not affect which
  // suggestions are allowed to display the chip. Does nothing if
  // `omnibox_action` is disabled.
  bool omnibox_action_on_navigation_intents = false;

  // If `omnibox_action_on_navigation_intents` is false, this threshold
  // helps determine when the user is intending to perform a navigation.
  int omnibox_action_navigation_intent_score_threshold = 1300;

  // If enabled, allows the Omnibox Action chip to appear when the suggestions
  // contain pedals. Does nothing if `omnibox_action` is disabled.
  bool omnibox_action_with_pedals = false;

  // Enables `HistoryClusterProvider` to surface Journeys as a suggestion row
  // instead of an action chip. Enabling this won't actually disable
  // `omnibox_action_with_pedals`, but for user experiments, the intent is to
  // only have 1 enabled.
  bool omnibox_history_cluster_provider = false;

  // If enabled, adds the keywords of aliases for detected entity names to a
  // cluster.
  bool keyword_filter_on_entity_aliases = false;

  // If greater than 0, the max number of aliases to include in keywords. If <=
  // 0, all aliases will be included.
  size_t max_entity_aliases_in_keywords = 0;

  // If enabled, adds the keywords of categories for detected entities to a
  // cluster.
  bool keyword_filter_on_categories = false;

  // If enabled, adds the keywords of detected entities from noisy visits to a
  // cluster.
  bool keyword_filter_on_noisy_visits = false;

  // If enabled, adds the search terms of the visits that have them.
  bool keyword_filter_on_search_terms = false;

  // If enabled, adds the keywords of detected entities that may be for
  // the visit's host.
  bool keyword_filter_on_visit_hosts = true;

  // The weight for category keyword scores per cluster.
  float category_keyword_score_weight = 1.0;

  // Maximum number of keywords to keep per cluster.
  size_t max_num_keywords_per_cluster = 20;

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

  // The minimum threshold for whether an entity is considered relevant to the
  // visit.
  int entity_relevance_threshold = 60;

  // The minimum threshold for whether a category is considered relevant to the
  // visit.
  int category_relevance_threshold = 36;  // 60 * 0.6 = 36.

  // Returns whether content clustering is enabled and
  // should be performed by the clustering backend.
  bool content_clustering_enabled = false;

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

  // Whether to hide clusters that only contain URLs from the same domain on
  // prominent UI surfaces.
  bool should_hide_single_domain_clusters_on_prominent_ui_surfaces = false;

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

  // Returns the default batch size for annotating visits when clustering.
  size_t clustering_tasks_batch_size = 250;

  // Whether to split the clusters when a search visit is encountered.
  bool split_clusters_at_search_visits = true;

  // Whether to assign labels to clusters. If the label exists, it will be shown
  // in the UI. If the label doesn't exist, the UI will emphasize the top visit.
  // Note: The default value here is meaningless, because the actual default
  // value is derived from the base::Feature.
  bool should_label_clusters = true;

  // Whether to assign labels to clusters from the hostnames of the cluster.
  // Does nothing if `should_label_clusters` is false. Note that since every
  // cluster has a hostname, this flag in conjunction with
  // `should_label_clusters` will give every cluster a label.
  bool labels_from_hostnames = true;

  // Whether to assign labels to clusters from the Entities of the cluster.
  // Does nothing if `should_label_clusters` is false.
  bool labels_from_entities = false;

  // Whether to check if all visits for a host should be in resulting clusters.
  bool should_check_hosts_to_skip_clustering_for = false;

  // The max number of hosts that should be stored in the engagement score
  // cache.
  int engagement_score_cache_size = 100;

  // The max time a host should be stored in the engagement score cache.
  base::TimeDelta engagement_score_cache_refresh_duration = base::Minutes(120);

  // True if the task runner should use trait CONTINUE_ON_SHUTDOWN.
  bool use_continue_on_shutdown = true;

  Config();
  Config(const Config& other);
  ~Config();
};

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

// Gets the current configuration. OverrideWithFinch() must have been called
// before GetConfig() is called.
const Config& GetConfig();

// Overrides the config returned by |GetConfig()|.
void SetConfigForTesting(const Config& config);

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_
