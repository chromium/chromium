// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/score_normalizing_ranker.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/util/score_normalizer.pb.h"

namespace app_list {
namespace {

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

ScoreNormalizingRanker::ScoreNormalizingRanker(
    PersistentProto<ScoreNormalizerProto> proto) {}

ScoreNormalizingRanker::~ScoreNormalizingRanker() {}

void ScoreNormalizingRanker::UpdateResultRanks(ResultsMap& results,
                                               ProviderType provider) {}

}  // namespace app_list
