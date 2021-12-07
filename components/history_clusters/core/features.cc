// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/features.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"

namespace history_clusters {

namespace {

constexpr auto enabled_by_default_desktop_only =
#if defined(OS_ANDROID) || defined(OS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

}  // namespace

bool IsJourneysEnabled(const std::string& locale) {
  if (!base::FeatureList::IsEnabled(kJourneys))
    return false;

  // Allow comma and colon as delimiters to the language list.
  auto allowlist =
      base::SplitString(kLocaleOrLanguageAllowlist.Get(),
                        ",:", base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);
  if (allowlist.empty())
    return true;

  // Allow any exact locale matches, and also allow any users where the primary
  // language subtag, e.g. "en" from "en-US" to match any element of the list.
  return base::Contains(allowlist, locale) ||
         base::Contains(allowlist, l10n_util::GetLanguage(locale));
}

const base::FeatureParam<std::string> kLocaleOrLanguageAllowlist{
    &kJourneys, "JourneysLocaleOrLanguageAllowlist", ""};

const base::FeatureParam<int> kMaxVisitsToCluster{
    &kJourneys, "JourneysMaxVisitsToCluster", 1000};

const base::FeatureParam<int> kMaxDaysToCluster{&kJourneys,
                                                "JourneysMaxDaysToCluster", 9};

// 20k should be more than enough for most cases and should avoid consuming
// large amounts of memory in extreme cases.
const base::FeatureParam<int> kMaxKeywordPhrases{
    &kJourneys, "JourneysMaxKeywordPhrases", 20000};

const base::FeatureParam<bool> kPersistClustersInHistoryDb{
    &kJourneys, "JourneysPersistClustersInHistoryDb", false};

const base::FeatureParam<double> kMinScoreToAlwaysShowAboveTheFold{
    &kJourneys, "JourneysMinScoreToAlwaysShowAboveTheFold", 0.5};

const base::FeatureParam<int> kNumVisitsToAlwaysShowAboveTheFold{
    &kJourneys, "JourneysNumVisitsToAlwaysShowAboveTheFold", 3};

// Default to true, as this new alternate action text was recommended by our UX
// writers.
const base::FeatureParam<bool> kAlternateOmniboxActionText{
    &kOmniboxAction, "JourneysAlternateOmniboxActionText", true};

const base::Feature kJourneys{"Journeys", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOmniboxAction{"JourneysOmniboxAction",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNonUserVisibleDebug{"JourneysNonUserVisibleDebug",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUserVisibleDebug{"JourneysUserVisibleDebug",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPersistContextAnnotationsInHistoryDb{
    "JourneysPersistContextAnnotationsInHistoryDb",
    enabled_by_default_desktop_only};

}  // namespace history_clusters
