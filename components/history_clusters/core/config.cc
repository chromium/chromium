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

  rescore_visits_within_clusters_for_query =
      base::GetFieldTrialParamByFeatureAsBool(
          internal::kJourneys, "JourneysRescoreVisitsWithinClustersForQuery",
          rescore_visits_within_clusters_for_query);

  alternate_omnibox_action_text = base::GetFieldTrialParamByFeatureAsBool(
      internal::kJourneys, "JourneysAlternateOmniboxActionText",
      alternate_omnibox_action_text);

  omnibox_action = base::FeatureList::IsEnabled(internal::kOmniboxAction);

  non_user_visible_debug =
      base::FeatureList::IsEnabled(internal::kNonUserVisibleDebug);

  user_visible_debug =
      base::FeatureList::IsEnabled(internal::kUserVisibleDebug);

  persist_context_annotations_in_history_db = base::FeatureList::IsEnabled(
      internal::kPersistContextAnnotationsInHistoryDb);

  history_clusters_internals_page =
      base::FeatureList::IsEnabled(internal::kHistoryClustersInternalsPage);
}

Config::Config(const Config& other) = default;
Config::~Config() = default;

void SetConfigForTesting(const Config& config) {
  const_cast<Config&>(GetConfig()) = config;
}

bool IsApplicationLocaleSupportedByJourneys(
    const std::string& application_locale) {
  // Application locale support should be checked only if the Journeys feature
  // is enabled.
  DCHECK(GetConfig().is_journeys_enabled_no_locale_check);

  // Default to "", because defaulting it to a specific locale makes it hard
  // to allow all locales, since the FeatureParam code interprets an empty
  // string as undefined, and instead returns the default value.
  const base::FeatureParam<std::string> kLocaleOrLanguageAllowlist{
      &internal::kJourneys, "JourneysLocaleOrLanguageAllowlist", ""};

  // Allow comma and colon as delimiters to the language list.
  auto allowlist =
      base::SplitString(kLocaleOrLanguageAllowlist.Get(),
                        ",:", base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);

  // Allow any exact locale matches, and also allow any users where the
  // primary language subtag, e.g. "en" from "en-US" to match any element of
  // the list.
  return allowlist.empty() || base::Contains(allowlist, application_locale) ||
         base::Contains(allowlist, l10n_util::GetLanguage(application_locale));
}

const Config& GetConfig() {
  static base::NoDestructor<Config> s_config;
  return *s_config;
}

}  // namespace history_clusters
