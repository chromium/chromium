// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/config.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace history_clusters {

namespace {

Config& GetConfigInternal() {
  static base::NoDestructor<Config> s_config;
  return *s_config;
}

}  // namespace

Config::Config() {
  // Override any parameters that may be provided by Finch.
  is_journeys_enabled_no_locale_check =
      base::FeatureList::IsEnabled(internal::kJourneys);

  max_visits_to_cluster = base::GetFieldTrialParamByFeatureAsInt(
      internal::kJourneys, "JourneysMaxVisitsToCluster", max_visits_to_cluster);

  max_days_to_cluster = base::GetFieldTrialParamByFeatureAsInt(
      internal::kJourneys, "JourneysMaxDaysToCluster", max_days_to_cluster);

  max_keyword_phrases = base::GetFieldTrialParamByFeatureAsInt(
      internal::kJourneys, "JourneysMaxKeywordPhrases", max_keyword_phrases);

  persist_clusters_in_history_db = base::GetFieldTrialParamByFeatureAsBool(
      internal::kJourneys, "JourneysPersistClustersInHistoryDb",
      persist_clusters_in_history_db);

  min_score_to_always_show_above_the_fold =
      base::GetFieldTrialParamByFeatureAsDouble(
          internal::kJourneys, "JourneysMinScoreToAlwaysShowAboveTheFold",
          min_score_to_always_show_above_the_fold);

  num_visits_to_always_show_above_the_fold =
      base::GetFieldTrialParamByFeatureAsInt(
          internal::kJourneys, "JourneysNumVisitsToAlwaysShowAboveTheFold",
          num_visits_to_always_show_above_the_fold);

  drop_hidden_visits = base::GetFieldTrialParamByFeatureAsBool(
      internal::kJourneys, "drop_hidden_visits", drop_hidden_visits);

  rescore_visits_within_clusters_for_query =
      base::GetFieldTrialParamByFeatureAsBool(
          internal::kJourneys, "JourneysRescoreVisitsWithinClustersForQuery",
          rescore_visits_within_clusters_for_query);

  sort_clusters_within_batch_for_query =
      base::GetFieldTrialParamByFeatureAsBool(
          internal::kJourneys, "JourneysSortClustersWithinBatchForQuery",
          sort_clusters_within_batch_for_query);

  omnibox_action = base::FeatureList::IsEnabled(internal::kOmniboxAction);

  omnibox_action_on_urls = base::GetFieldTrialParamByFeatureAsBool(
      internal::kOmniboxAction, "omnibox_action_on_urls",
      omnibox_action_on_urls);

  omnibox_action_on_noisy_urls = base::GetFieldTrialParamByFeatureAsBool(
      internal::kOmniboxAction, "omnibox_action_on_noisy_urls",
      omnibox_action_on_noisy_urls);

  non_user_visible_debug =
      base::FeatureList::IsEnabled(internal::kNonUserVisibleDebug);

  user_visible_debug =
      base::FeatureList::IsEnabled(internal::kUserVisibleDebug);

  persist_context_annotations_in_history_db = base::FeatureList::IsEnabled(
      internal::kPersistContextAnnotationsInHistoryDb);

  history_clusters_internals_page =
      base::FeatureList::IsEnabled(internal::kHistoryClustersInternalsPage);

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

  category_relevance_threshold = GetFieldTrialParamByFeatureAsInt(
      features::kOnDeviceClustering, "category_relevance_threshold",
      category_relevance_threshold);
  // Ensure that the value is [0 and 100].
  DCHECK_GE(category_relevance_threshold, 0);
  DCHECK_LE(category_relevance_threshold, 100);

  content_clustering_enabled = GetFieldTrialParamByFeatureAsBool(
      features::kOnDeviceClustering, "content_clustering_enabled",
      content_clustering_enabled);

  content_clustering_entity_similarity_weight =
      GetFieldTrialParamByFeatureAsDouble(
          features::kOnDeviceClustering,
          "content_clustering_entity_similarity_weight",
          content_clustering_entity_similarity_weight);

  content_clustering_category_similarity_weight =
      GetFieldTrialParamByFeatureAsDouble(
          features::kOnDeviceClustering,
          "content_clustering_category_similarity_weight",
          content_clustering_category_similarity_weight);

  content_clustering_similarity_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kOnDeviceClustering, "content_clustering_similarity_threshold",
      content_clustering_similarity_threshold);
  // Ensure that the value is [0.0 and 1.0].
  DCHECK_GE(content_clustering_similarity_threshold, 0.0f);
  DCHECK_LE(content_clustering_similarity_threshold, 1.0f);

  content_visibility_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kOnDeviceClustering, "content_visibility_threshold", 0.7);
  // Ensure that the value is [0.0 and 1.0].
  DCHECK_GE(content_visibility_threshold, 0.0f);
  DCHECK_LE(content_visibility_threshold, 1.0f);

  should_hide_single_visit_clusters_on_prominent_ui_surfaces =
      GetFieldTrialParamByFeatureAsBool(
          features::kOnDeviceClustering,
          "hide_single_visit_clusters_on_prominent_ui_surfaces",
          should_hide_single_visit_clusters_on_prominent_ui_surfaces);

  should_dedupe_similar_visits = GetFieldTrialParamByFeatureAsBool(
      features::kOnDeviceClustering, "dedupe_similar_visits",
      should_dedupe_similar_visits);

  should_filter_noisy_clusters = GetFieldTrialParamByFeatureAsBool(
      features::kOnDeviceClustering, "filter_noisy_clusters",
      should_filter_noisy_clusters);

  noisy_cluster_visits_engagement_threshold =
      GetFieldTrialParamByFeatureAsDouble(
          features::kOnDeviceClustering,
          "noisy_cluster_visit_engagement_threshold",
          noisy_cluster_visits_engagement_threshold);

  number_interesting_visits_filter_threshold = GetFieldTrialParamByFeatureAsInt(
      features::kOnDeviceClustering, "num_interesting_visits_filter_threshold",
      number_interesting_visits_filter_threshold);

  visit_duration_ranking_weight = GetFieldTrialParamByFeatureAsDouble(
      features::kOnDeviceClustering, "visit_duration_ranking_weight",
      visit_duration_ranking_weight);
  DCHECK_GE(visit_duration_ranking_weight, 0.0f);

  foreground_duration_ranking_weight = GetFieldTrialParamByFeatureAsDouble(
      features::kOnDeviceClustering, "foreground_duration_ranking_weight",
      foreground_duration_ranking_weight);
  DCHECK_GE(foreground_duration_ranking_weight, 0.0f);

  bookmark_ranking_weight = GetFieldTrialParamByFeatureAsDouble(
      features::kOnDeviceClustering, "bookmark_ranking_weight",
      bookmark_ranking_weight);
  DCHECK_GE(bookmark_ranking_weight, 0.0f);

  search_results_page_ranking_weight = GetFieldTrialParamByFeatureAsDouble(
      features::kOnDeviceClustering, "search_results_page_ranking_weight",
      search_results_page_ranking_weight);
  DCHECK_GE(search_results_page_ranking_weight, 0.0f);

  has_page_title_ranking_weight = GetFieldTrialParamByFeatureAsDouble(
      features::kOnDeviceClustering, "has_page_title_ranking_weight",
      has_page_title_ranking_weight);
  DCHECK_GE(has_page_title_ranking_weight, 0.0f);

  content_cluster_on_intersection_similarity =
      GetFieldTrialParamByFeatureAsBool(
          features::kOnDeviceClustering,
          "use_content_clustering_intersection_similarity",
          content_cluster_on_intersection_similarity);

  cluster_interaction_threshold = GetFieldTrialParamByFeatureAsInt(
      features::kOnDeviceClustering,
      "content_clustering_intersection_threshold",
      cluster_interaction_threshold);

  should_include_categories_in_keywords = GetFieldTrialParamByFeatureAsBool(
      features::kOnDeviceClustering, "include_categories_in_keywords",
      should_include_categories_in_keywords);

  should_exclude_keywords_from_noisy_visits = GetFieldTrialParamByFeatureAsBool(
      features::kOnDeviceClustering, "exclude_keywords_from_noisy_visits",
      should_exclude_keywords_from_noisy_visits);

  clustering_tasks_batch_size = GetFieldTrialParamByFeatureAsInt(
      features::kSplitClusteringTasksToSmallerBatches,
      "clustering_task_batch_size", clustering_tasks_batch_size);

  split_clusters_at_search_visits = GetFieldTrialParamByFeatureAsBool(
      features::kOnDeviceClustering, "split_clusters_at_search_visits",
      split_clusters_at_search_visits);

  should_label_clusters = GetFieldTrialParamByFeatureAsBool(
      features::kOnDeviceClustering, "should_label_clusters",
      should_label_clusters);

  labels_from_hostnames = GetFieldTrialParamByFeatureAsBool(
      features::kOnDeviceClustering, "labels_from_hostnames",
      labels_from_hostnames);

  labels_from_entities = GetFieldTrialParamByFeatureAsBool(
      features::kOnDeviceClustering, "labels_from_entities",
      labels_from_entities);

  const base::FeatureParam<std::string> kHostsToSkipClusteringFor{
      &features::kOnDeviceClusteringBlocklists, "hosts_to_skip_clustering_for",
      ""};
  auto hosts = base::SplitString(kHostsToSkipClusteringFor.Get(), ",",
                                 base::WhitespaceHandling::TRIM_WHITESPACE,
                                 base::SplitResult::SPLIT_WANT_NONEMPTY);
  hosts_to_skip_clustering_for = {hosts.begin(), hosts.end()};

  use_continue_on_shutdown = base::FeatureList::IsEnabled(
      internal::kHistoryClustersUseContinueOnShutdown);
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
