// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/config.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
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

  // The `kJourneys` feature and child params.
  {
    is_journeys_enabled_no_locale_check =
        base::FeatureList::IsEnabled(internal::kJourneys);

    max_visits_to_cluster = base::GetFieldTrialParamByFeatureAsInt(
        internal::kJourneys, "JourneysMaxVisitsToCluster",
        max_visits_to_cluster);

    max_keyword_phrases = base::GetFieldTrialParamByFeatureAsInt(
        internal::kJourneys, "JourneysMaxKeywordPhrases", max_keyword_phrases);

    min_score_to_always_show_above_the_fold =
        base::GetFieldTrialParamByFeatureAsDouble(
            internal::kJourneys, "JourneysMinScoreToAlwaysShowAboveTheFold",
            min_score_to_always_show_above_the_fold);

    num_visits_to_always_show_above_the_fold =
        base::GetFieldTrialParamByFeatureAsInt(
            internal::kJourneys, "JourneysNumVisitsToAlwaysShowAboveTheFold",
            num_visits_to_always_show_above_the_fold);

    rescore_visits_within_clusters_for_query =
        base::GetFieldTrialParamByFeatureAsBool(
            internal::kJourneys, "JourneysRescoreVisitsWithinClustersForQuery",
            rescore_visits_within_clusters_for_query);

    sort_clusters_within_batch_for_query =
        base::GetFieldTrialParamByFeatureAsBool(
            internal::kJourneys, "JourneysSortClustersWithinBatchForQuery",
            sort_clusters_within_batch_for_query);
  }

  // The `kJourneysImages` feature.
  {
    images = base::FeatureList::IsEnabled(internal::kJourneysImages);

    images_cover = GetFieldTrialParamByFeatureAsBool(
        internal::kJourneysImages, "JourneysImagesCover", images_cover);
  }

  // The `kOmniboxAction` feature and child params.
  {
    omnibox_action = base::FeatureList::IsEnabled(internal::kOmniboxAction);

    omnibox_action_with_pedals = base::GetFieldTrialParamByFeatureAsBool(
        internal::kOmniboxAction, "omnibox_action_with_pedals",
        omnibox_action_with_pedals);

    omnibox_action_on_navigation_intents =
        base::GetFieldTrialParamByFeatureAsBool(
            internal::kOmniboxAction, "omnibox_action_on_navigation_intents",
            omnibox_action_on_navigation_intents);

    omnibox_action_navigation_intent_score_threshold =
        base::GetFieldTrialParamByFeatureAsInt(
            internal::kOmniboxAction,
            "omnibox_action_on_navigation_intent_score_threshold",
            omnibox_action_navigation_intent_score_threshold);
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

  // The `kUseEngagementScoreCache` feature and child params.
  {
    use_engagement_score_cache =
        base::FeatureList::IsEnabled(features::kUseEngagementScoreCache);

    engagement_score_cache_size = GetFieldTrialParamByFeatureAsInt(
        features::kUseEngagementScoreCache, "engagement_score_cache_size",
        engagement_score_cache_size);

    engagement_score_cache_refresh_duration =
        base::Minutes(GetFieldTrialParamByFeatureAsInt(
            features::kUseEngagementScoreCache,
            "engagement_score_cache_refresh_duration_minutes",
            engagement_score_cache_refresh_duration.InMinutes()));
  }

  // The `kHistoryClustersVisitDeduping` feature and child params.
  {
    use_host_for_visit_deduping = GetFieldTrialParamByFeatureAsBool(
        internal::kHistoryClustersVisitDeduping, "use_host_for_visit_deduping",
        use_host_for_visit_deduping);
  }

  // The `kOnDeviceClusteringVisitRanking` feature and child params.
  {
    visit_duration_ranking_weight = GetFieldTrialParamByFeatureAsDouble(
        features::kOnDeviceClusteringVisitRanking,
        "visit_duration_ranking_weight", visit_duration_ranking_weight);
    DCHECK_GE(visit_duration_ranking_weight, 0.0f);

    foreground_duration_ranking_weight = GetFieldTrialParamByFeatureAsDouble(
        features::kOnDeviceClusteringVisitRanking,
        "foreground_duration_ranking_weight",
        foreground_duration_ranking_weight);
    DCHECK_GE(foreground_duration_ranking_weight, 0.0f);

    bookmark_ranking_weight = GetFieldTrialParamByFeatureAsDouble(
        features::kOnDeviceClusteringVisitRanking, "bookmark_ranking_weight",
        bookmark_ranking_weight);
    DCHECK_GE(bookmark_ranking_weight, 0.0f);

    search_results_page_ranking_weight = GetFieldTrialParamByFeatureAsDouble(
        features::kOnDeviceClusteringVisitRanking,
        "search_results_page_ranking_weight",
        search_results_page_ranking_weight);
    DCHECK_GE(search_results_page_ranking_weight, 0.0f);

    has_url_keyed_image_ranking_weight = GetFieldTrialParamByFeatureAsDouble(
        features::kOnDeviceClusteringVisitRanking,
        "has_url_keyed_image_ranking_weight",
        has_url_keyed_image_ranking_weight);
    DCHECK_GE(has_url_keyed_image_ranking_weight, 0.0f);
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

  // WebUI features and params.
  {
    named_new_tab_groups =
        base::FeatureList::IsEnabled(internal::kJourneysNamedNewTabGroups);
  }

  // The `kJourneysZeroStateFiltering` feature and child params.
  {
    apply_zero_state_filtering =
        base::FeatureList::IsEnabled(internal::kJourneysZeroStateFiltering);
  }

  // Lonely features without child params.
  {
    non_user_visible_debug =
        base::FeatureList::IsEnabled(internal::kNonUserVisibleDebug);

    user_visible_debug =
        base::FeatureList::IsEnabled(internal::kUserVisibleDebug);

    persist_context_annotations_in_history_db = base::FeatureList::IsEnabled(
        internal::kPersistContextAnnotationsInHistoryDb);

    history_clusters_internals_page =
        base::FeatureList::IsEnabled(internal::kHistoryClustersInternalsPage);

    should_check_hosts_to_skip_clustering_for =
        base::FeatureList::IsEnabled(features::kOnDeviceClusteringBlocklists);

    use_continue_on_shutdown = base::FeatureList::IsEnabled(
        internal::kHistoryClustersUseContinueOnShutdown);

    should_show_all_clusters_unconditionally_on_prominent_ui_surfaces =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kShouldShowAllClustersOnProminentUiSurfaces);

    persist_caches_to_prefs =
        base::FeatureList::IsEnabled(internal::kJourneysPersistCachesToPrefs);
  }
}

Config::Config(const Config& other) = default;
Config::~Config() = default;

void SetConfigForTesting(const Config& config) {
  GetConfigInternal() = config;
}

bool IsApplicationLocaleSupportedByJourneys(
    const std::string& application_locale) {
  // Application locale support should be checked only if the Journeys feature
  // is enabled.
  DCHECK(GetConfig().is_journeys_enabled_no_locale_check);

  // Note, we now set a default value for the allowlist, which means that when
  // the feature parameter is undefined, the below allowlist is enabled.
  const base::FeatureParam<std::string> kLocaleOrLanguageAllowlist{
      &internal::kJourneys, "JourneysLocaleOrLanguageAllowlist",
      "de:en:es:fr:it:nl:pt:tr"};

  // To allow for using any locale, we also interpret the special '*' value.
  auto allowlist_string = kLocaleOrLanguageAllowlist.Get();
  if (allowlist_string == "*")
    return true;

  // Allow comma and colon as delimiters to the language list.
  auto allowlist = base::SplitString(
      allowlist_string, ",:", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);

  // Allow any exact locale matches, and also allow any users where the
  // primary language subtag, e.g. "en" from "en-US" to match any element of
  // the list.
  return allowlist.empty() || base::Contains(allowlist, application_locale) ||
         base::Contains(allowlist, l10n_util::GetLanguage(application_locale));
}

const Config& GetConfig() {
  return GetConfigInternal();
}

}  // namespace history_clusters
