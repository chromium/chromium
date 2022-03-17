// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/types.h"

#include <stddef.h>

#include "ash/constants/ash_features.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace app_list {

double Scoring::FinalScore() const {
  if (filter)
    return -1.0;
  return ftrl_result_score;
}

double Scoring::BestMatchScore() const {
  if (filter)
    return -1.0;

  if (base::GetFieldTrialParamByFeatureAsBool(
          ash::features::kProductivityLauncher, "best_match_usage", true)) {
    return std::max(mrfu_result_score, normalized_relevance);
  } else {
    return normalized_relevance;
  }
}

::std::ostream& operator<<(::std::ostream& os, const Scoring& scoring) {
  if (scoring.filter)
    return os << "{" << scoring.FinalScore() << " | filtered}";
  return os << base::StringPrintf(
             "{%.2f | nr:%.2f rs:%.2f bm:%d cr:%d bi:%d}", scoring.FinalScore(),
             scoring.normalized_relevance, scoring.ftrl_result_score,
             scoring.best_match_rank, scoring.continue_rank,
             scoring.burnin_iteration);
}

CategoriesList CreateAllCategories() {
  CategoriesList res({{.category = Category::kApps},
                      {.category = Category::kAppShortcuts},
                      {.category = Category::kWeb},
                      {.category = Category::kFiles},
                      {.category = Category::kSettings},
                      {.category = Category::kHelp},
                      {.category = Category::kPlayStore},
                      {.category = Category::kSearchAndAssistant},
                      {.category = Category::kGames}});
  DCHECK_EQ(res.size(), static_cast<size_t>(Category::kMaxValue));
  return res;
}

}  // namespace app_list
