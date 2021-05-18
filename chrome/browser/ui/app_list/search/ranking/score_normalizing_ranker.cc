// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/score_normalizing_ranker.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

namespace app_list {
namespace {

// How many bins to use for score normalization.
constexpr int kNormalizationBins = 20;

// Prefix added to the name of each score normalizer, which is used for prefs
// storage.
constexpr char kNormalizerPrefix[] = "categorical_search_normalizer_";

// Returns true if results from this provider should not have their result
// scores normalized. This is to prevent creating an unnecessary number of
// normalizers.
bool ShouldIgnoreProvider(ProviderType type) {
  switch (type) {
      // Deprecated types:
    case ProviderType::kLauncher:
    case ProviderType::kAnswerCard:
      // Types not associated with a provider:
    case ProviderType::kFileChip:
    case ProviderType::kDriveChip:
      // Types that only create suggestion chips:
    case ProviderType::kAssistantChip:
      // Types that only ever create one result:
    case ProviderType::kPlayStoreReinstallApp:
      // Internal types:
    case ProviderType::kUnknown:
    case ProviderType::kInternalPrivacyInfo:
      return true;
    default:
      return false;
  }
}

}  // namespace

ScoreNormalizingRanker::ScoreNormalizingRanker(Profile* profile) {
  static constexpr int kProviderMin = static_cast<int>(ProviderType::kUnknown);
  static constexpr int kProviderMax = static_cast<int>(ProviderType::kMaxValue);

  for (int provider_int = kProviderMin; provider_int <= kProviderMax;
       ++provider_int) {
    const ProviderType provider = static_cast<ProviderType>(provider_int);
    if (ShouldIgnoreProvider(provider))
      continue;

    const std::string name =
        base::StrCat({kNormalizerPrefix, base::NumberToString(provider_int)});
    normalizers_[provider] =
        std::make_unique<ScoreNormalizer>(name, profile, kNormalizationBins);
  }
}

ScoreNormalizingRanker::~ScoreNormalizingRanker() {}

void ScoreNormalizingRanker::Rank(ResultsMap& results, ProviderType provider) {
  const auto it = normalizers_.find(provider);

  // Early exit if this isn't a provider that we normalize.
  if (it == normalizers_.end())
    return;

  // First update the provider's normalizer with the new scores, then normalize
  // the scores themselves. This is normalizing all results from a provider,
  // including those that will be shown in different UI surfaces. In particular,
  // this also normalizes suggestion chips. This has no effect because the
  // ranking of the suggestion chips is fixed regardless of inter-provider
  // score.
  const auto provider_results = results.find(provider);
  DCHECK(provider_results != results.end());
  it->second->RecordResults(provider_results->second);
  it->second->NormalizeResults(&provider_results->second);
}

}  // namespace app_list
