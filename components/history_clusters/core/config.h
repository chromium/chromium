// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_

#include <string>

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
