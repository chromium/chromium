// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/config.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/search/ntp_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace history_clusters {

namespace {

Config& GetConfigInternal() {
  static base::NoDestructor<Config> s_config;
  return *s_config;
}

}  // namespace

namespace switches {

const char kShouldShowAllClustersOnProminentUiSurfaces[] =
    "history-clusters-should-show-all-clusters-on-prominent-ui-surfaces";

}  // namespace switches

Config::Config() {
  // Override any parameters that may be provided by Finch.

  // The `kJourneysImages` feature.
  {
    images = base::FeatureList::IsEnabled(internal::kJourneysImages);

    images_cover = GetFieldTrialParamByFeatureAsBool(
        internal::kJourneysImages, "JourneysImagesCover", images_cover);
  }

  // The `kOmniboxHistoryClusterProvider` feature and child params.
  {
    omnibox_history_cluster_provider =
        base::FeatureList::IsEnabled(internal::kOmniboxHistoryClusterProvider);

    omnibox_history_cluster_provider_counterfactual =
        base::GetFieldTrialParamByFeatureAsBool(
            internal::kOmniboxHistoryClusterProvider,
            "omnibox_history_cluster_provider_counterfactual",
            omnibox_history_cluster_provider_counterfactual);

    omnibox_history_cluster_provider_score =
        base::GetFieldTrialParamByFeatureAsInt(
            internal::kOmniboxHistoryClusterProvider,
            "omnibox_history_cluster_provider_score",
            omnibox_history_cluster_provider_score);

    omnibox_history_cluster_provider_inherit_search_match_score =
        base::GetFieldTrialParamByFeatureAsBool(
            internal::kOmniboxHistoryClusterProvider,
            "omnibox_history_cluster_provider_inherit_search_match_score",
            omnibox_history_cluster_provider_inherit_search_match_score);

    omnibox_history_cluster_provider_navigation_intent_score_threshold =
        base::GetFieldTrialParamByFeatureAsInt(
            internal::kOmniboxHistoryClusterProvider,
            "omnibox_history_cluster_provider_navigation_intent_score_"
            "threshold",
            omnibox_history_cluster_provider_navigation_intent_score_threshold);

    omnibox_history_cluster_provider_on_navigation_intents =
        base::GetFieldTrialParamByFeatureAsBool(
            internal::kOmniboxHistoryClusterProvider,
            "omnibox_history_cluster_provider_on_navigation_intents",
            omnibox_history_cluster_provider_on_navigation_intents);
  }

  // The `kOnDeviceClusteringKeywordFiltering` feature and child params.
  {
    keyword_filter_on_entity_aliases = base::GetFieldTrialParamByFeatureAsBool(
        history_clusters::features::kOnDeviceClusteringKeywordFiltering,
        "keyword_filter_on_entity_aliases", keyword_filter_on_entity_aliases);

    max_entity_aliases_in_keywords = base::GetFieldTrialParamByFeatureAsInt(
        history_clusters::features::kOnDeviceClusteringKeywordFiltering,
        "max_entity_aliases_in_keywords", max_entity_aliases_in_keywords);
    if (max_entity_aliases_in_keywords <= 0) {
      max_entity_aliases_in_keywords = SIZE_MAX;
    }

    keyword_filter_on_noisy_visits = GetFieldTrialParamByFeatureAsBool(
        history_clusters::features::kOnDeviceClusteringKeywordFiltering,
        "keyword_filter_on_noisy_visits", keyword_filter_on_noisy_visits);

    max_num_keywords_per_cluster = GetFieldTrialParamByFeatureAsInt(
        features::kOnDeviceClusteringKeywordFiltering,
        "max_num_keywords_per_cluster", max_num_keywords_per_cluster);
  }

  // The `kOnDeviceClustering` feature and child params.
  {
    cluster_navigation_time_cutoff =
        base::Minutes(GetFieldTrialParamByFeatureAsInt(
            features::kOnDeviceClustering, "navigation_time_cutoff_minutes",
            cluster_navigation_time_cutoff.InMinutes()));

    entity_relevance_threshold = GetFieldTrialParamByFeatureAsInt(
        features::kOnDeviceClustering, "entity_relevance_threshold",
        entity_relevance_threshold);
    // Ensure that the value is [0 and 100].
    DCHECK_GE(entity_relevance_threshold, 0);
    DCHECK_LE(entity_relevance_threshold, 100);

    content_visibility_threshold = GetFieldTrialParamByFeatureAsDouble(
        features::kOnDeviceClustering, "content_visibility_threshold", 0.5);
    // Ensure that the value is [0.0 and 1.0].
    DCHECK_GE(content_visibility_threshold, 0.0f);
    DCHECK_LE(content_visibility_threshold, 1.0f);

    noisy_cluster_visits_engagement_threshold =
        GetFieldTrialParamByFeatureAsDouble(
            features::kOnDeviceClustering,
            "noisy_cluster_visit_engagement_threshold",
            noisy_cluster_visits_engagement_threshold);

    number_interesting_visits_filter_threshold =
        GetFieldTrialParamByFeatureAsInt(
            features::kOnDeviceClustering,
            "num_interesting_visits_filter_threshold",
            number_interesting_visits_filter_threshold);
  }

  // The `kHistoryClustersVisitDeduping` feature and child params.
  {
    use_host_for_visit_deduping = GetFieldTrialParamByFeatureAsBool(
        internal::kHistoryClustersVisitDeduping, "use_host_for_visit_deduping",
        use_host_for_visit_deduping);
  }

  // The `kHistoryClustersNavigationContextClustering` feature and child params.
  {
    use_navigation_context_clusters = base::FeatureList::IsEnabled(
        internal::kHistoryClustersNavigationContextClustering);

    context_clustering_clean_up_duration =
        base::Minutes(GetFieldTrialParamByFeatureAsInt(
            internal::kHistoryClustersNavigationContextClustering,
            "clean_up_duration_minutes",
            context_clustering_clean_up_duration.InMinutes()));

    cluster_triggerability_cutoff_duration =
        base::Minutes(GetFieldTrialParamByFeatureAsInt(
            internal::kHistoryClustersNavigationContextClustering,
            "cluster_triggerability_cutoff_duration_minutes",
            cluster_triggerability_cutoff_duration.InMinutes()));
  }

  // Lonely features without child params.
  {
    non_user_visible_debug =
        base::FeatureList::IsEnabled(internal::kNonUserVisibleDebug);

    user_visible_debug =
        base::FeatureList::IsEnabled(internal::kUserVisibleDebug);

    history_clusters_internals_page =
        base::FeatureList::IsEnabled(internal::kHistoryClustersInternalsPage);

    should_check_hosts_to_skip_clustering_for =
        base::FeatureList::IsEnabled(features::kOnDeviceClusteringBlocklists);

    should_show_all_clusters_unconditionally_on_prominent_ui_surfaces =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kShouldShowAllClustersOnProminentUiSurfaces);
  }
}

Config::Config(const Config& other) = default;
Config::~Config() = default;

void SetConfigForTesting(const Config& config) {
  GetConfigInternal() = config;
}

const Config& GetConfig() {
  return GetConfigInternal();
}

}  // namespace history_clusters
