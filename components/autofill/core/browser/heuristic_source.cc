// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/heuristic_source.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

HeuristicSource GetActiveHeuristicSource() {
  if (base::FeatureList::IsEnabled(features::kAutofillModelPredictions)) {
    static bool model_predictions_active =
        features::kAutofillModelPredictionsAreActive.Get();
    if (model_predictions_active) {
      return HeuristicSource::kMachineLearning;
    }
  }
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  static const HeuristicSource active_source =
      GetActiveRegexFeatures().empty() ? HeuristicSource::kDefaultRegexes
                                       : HeuristicSource::kExperimentalRegexes;
  return active_source;
#else
  return HeuristicSource::kLegacyRegexes;
#endif
}

std::optional<PatternFile> HeuristicSourceToPatternFile(
    HeuristicSource source) {
  switch (source) {
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    case HeuristicSource::kLegacyRegexes:
      return PatternFile::kLegacy;
#else
    case HeuristicSource::kDefaultRegexes:
    case HeuristicSource::kExperimentalRegexes:
      return PatternFile::kDefault;
    case HeuristicSource::kPredictionImprovementRegexes:
      return PatternFile::kPredictionImprovements;
#endif
    case autofill::HeuristicSource::kMachineLearning:
      return std::nullopt;
  }
  NOTREACHED();
}

}  // namespace autofill
