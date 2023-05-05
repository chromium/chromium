// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/config.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/prefs/pref_service.h"
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

  // The `kJourneysLabels` feature and child params.
  {
    labels_from_hostnames = GetFieldTrialParamByFeatureAsBool(
        internal::kJourneysLabels, "labels_from_hostnames",
        labels_from_hostnames);

    labels_from_entities = GetFieldTrialParamByFeatureAsBool(
        internal::kJourneysLabels, "labels_from_entities",
        labels_from_entities);

    labels_from_search_visit_entities = GetFieldTrialParamByFeatureAsBool(
        internal::kJourneysLabels, "labels_from_search_visit_entities",
        labels_from_search_visit_entities);
  }

  // The `kJourneysImages` feature.
  {
    images = base::FeatureList::IsEnabled(internal::kJourneysImages);

    images_cover = GetFieldTrialParamByFeatureAsBool(
        internal::kJourneysImages, "JourneysImagesCover", images_cover);
  }

  // The `kPersistedClusters` feature and child params.
  {
    persist_clusters_in_history_db =
        base::FeatureList::IsEnabled(internal::kPersistedClusters);

    persist_clusters_in_history_db_after_startup_delay_minutes =
        base::GetFieldTrialParamByFeatureAsInt(
            internal::kPersistedClusters,
            "JourneysPersistClustersInHistoryDbAfterStartupDelayMinutes",
            persist_clusters_in_history_db_after_startup_delay_minutes);

    persist_clusters_in_history_db_period_minutes =
        base::GetFieldTrialParamByFeatureAsInt(
            internal::kPersistedClusters,
            "JourneysPersistClustersInHistoryDbPeriodMinutes",
            persist_clusters_in_history_db_period_minutes);

    persist_on_query = base::GetFieldTrialParamByFeatureAsBool(
        internal::kPersistedClusters, "persist_on_query", persist_on_query);

    max_persisted_clusters_to_fetch = base::GetFieldTrialParamByFeatureAsInt(
        internal::kPersistedClusters, "max_persisted_clusters_to_fetch",
        max_persisted_clusters_to_fetch);

    max_persisted_cluster_visits_to_fetch_soft_cap =
        base::GetFieldTrialParamByFeatureAsInt(
            internal::kPersistedClusters,
            "max_persisted_cluster_visits_to_fetch_soft_cap",
            max_persisted_cluster_visits_to_fetch_soft_cap);

    persist_clusters_recluster_window_days =
        base::GetFieldTrialParamByFeatureAsInt(
            internal::kPersistedClusters,
            "persist_clusters_recluster_window_days",
            persist_clusters_recluster_window_days);
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

    omnibox_history_cluster_provider_rank_above_searches =
        base::GetFieldTrialParamByFeatureAsBool(
            internal::kOmniboxHistoryClusterProvider,
            "omnibox_history_cluster_provider_rank_above_searches",
            omnibox_history_cluster_provider_rank_above_searches);

    omnibox_history_cluster_provider_shortcuts =
        base::GetFieldTrialParamByFeatureAsBool(
            internal::kOmniboxHistoryClusterProvider,
            "omnibox_history_cluster_provider_shortcuts",
            omnibox_history_cluster_provider_shortcuts);

    omnibox_history_cluster_provider_allow_default =
        base::GetFieldTrialParamByFeatureAsBool(
            internal::kOmniboxHistoryClusterProvider,
            "omnibox_history_cluster_provider_allow_default",
            omnibox_history_cluster_provider_allow_default);

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

  // The `kOnDeviceClusteringContentClustering` feature and child params.
  {
    content_clustering_enabled = base::FeatureList::IsEnabled(
        features::kOnDeviceClusteringContentClustering);

    content_clustering_search_visits_only = GetFieldTrialParamByFeatureAsBool(
        features::kOnDeviceClusteringContentClustering, "search_visits_only",
        content_clustering_search_visits_only);

    content_clustering_similarity_threshold =
        GetFieldTrialParamByFeatureAsDouble(
            features::kOnDeviceClusteringContentClustering,
            "content_clustering_similarity_threshold",
            content_clustering_similarity_threshold);
    // Ensure that the value is [0.0 and 1.0].
    DCHECK_GE(content_clustering_similarity_threshold, 0.0f);
    DCHECK_LE(content_clustering_similarity_threshold, 1.0f);

    exclude_entities_that_have_no_collections_from_content_clustering =
        GetFieldTrialParamByFeatureAsBool(
            features::kOnDeviceClusteringContentClustering,
            "exclude_entities_that_have_no_collections",
            exclude_entities_that_have_no_collections_from_content_clustering);

    collections_to_block_from_content_clustering =
        JourneysCollectionContentClusteringBlocklist(
            collections_to_block_from_content_clustering);

    use_pairwise_merge = GetFieldTrialParamByFeatureAsBool(
        features::kOnDeviceClusteringContentClustering, "use_pairwise_merge",
        use_pairwise_merge);

    max_pairwise_merge_iterations = GetFieldTrialParamByFeatureAsInt(
        features::kOnDeviceClusteringContentClustering,
        "max_pairwise_merge_iterations", max_pairwise_merge_iterations);
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
    hide_visits = base::FeatureList::IsEnabled(internal::kHideVisits);

    hide_visits_icon = GetFieldTrialParamByFeatureAsBool(
        internal::kHideVisits, "hide_visits_icon", hide_visits_icon);
  }

  // The `kUseUrlForDisplayCache` feature and child params.
  {
    use_url_for_display_cache =
        base::FeatureList::IsEnabled(internal::kUseUrlForDisplayCache);

    url_for_display_cache_size = GetFieldTrialParamByFeatureAsInt(
        internal::kUseUrlForDisplayCache, "url_for_display_cache_size",
        url_for_display_cache_size);
  }

  // The `kJourneysZeroStateFiltering` feature and child params.
  {
    apply_zero_state_filtering =
        base::FeatureList::IsEnabled(internal::kJourneysZeroStateFiltering);
  }

  // The `kNtpChromeCartInHistoryClusterModule` child params.
  {
    use_ntp_specific_intracluster_ranking = GetFieldTrialParamByFeatureAsBool(
        ntp_features::kNtpChromeCartInHistoryClusterModule,
        "use_ntp_specific_intracluster_ranking",
        use_ntp_specific_intracluster_ranking);

    ntp_visit_duration_ranking_weight = GetFieldTrialParamByFeatureAsDouble(
        ntp_features::kNtpChromeCartInHistoryClusterModule,
        "ntp_visit_duration_ranking_weight", ntp_visit_duration_ranking_weight);
    DCHECK_GE(ntp_visit_duration_ranking_weight, 0.0f);
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

    include_synced_visits =
        base::FeatureList::IsEnabled(internal::kJourneysIncludeSyncedVisits);

    persist_caches_to_prefs =
        base::FeatureList::IsEnabled(internal::kJourneysPersistCachesToPrefs);
  }
}

Config::Config(const Config& other) = default;
Config::~Config() = default;

void SetConfigForTesting(const Config& config) {
  GetConfigInternal() = config;
}

base::flat_set<std::string> JourneysCollectionContentClusteringBlocklist(
    const base::flat_set<std::string>& default_value) {
  const base::FeatureParam<std::string>
      kJourneysCollectionContentClusteringBlocklist{
          &features::kOnDeviceClusteringContentClustering,
          "collections_blocklist", ""};
  std::string blocklist_string =
      kJourneysCollectionContentClusteringBlocklist.Get();
  if (blocklist_string.empty())
    return default_value;

  auto blocklist = base::SplitString(blocklist_string, ",",
                                     base::WhitespaceHandling::TRIM_WHITESPACE,
                                     base::SplitResult::SPLIT_WANT_NONEMPTY);

  return blocklist.empty()
             ? default_value
             : base::flat_set<std::string>(blocklist.begin(), blocklist.end());
}

base::flat_set<std::string> JourneysMidBlocklist() {
  const base::FeatureParam<std::string> kJourneysMidBlocklist{
      &internal::kHistoryClustersKeywordFiltering, "JourneysMidBlocklist", ""};
  std::string blocklist_string = kJourneysMidBlocklist.Get();
  if (blocklist_string.empty())
    return {};

  auto blocklist = base::SplitString(blocklist_string, ",",
                                     base::WhitespaceHandling::TRIM_WHITESPACE,
                                     base::SplitResult::SPLIT_WANT_NONEMPTY);

  return blocklist.empty()
             ? base::flat_set<std::string>()
             : base::flat_set<std::string>(blocklist.begin(), blocklist.end());
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

bool IsJourneysEnabledInOmnibox(HistoryClustersService* service,
                                PrefService* prefs) {
  if (!service)
    return false;

  if (!service->IsJourneysEnabled())
    return false;

  if (!prefs->GetBoolean(history_clusters::prefs::kVisible))
    return false;

  return true;
}

const Config& GetConfig() {
  return GetConfigInternal();
}

}  // namespace history_clusters
