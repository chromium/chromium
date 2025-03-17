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
  if (base::FeatureList::IsEnabled(features::kAutofillModelPredictions) &&
      features::kAutofillModelPredictionsAreActive.Get()) {
    return HeuristicSource::kAutofillMachineLearning;
  }
  return HeuristicSource::kRegexes;
}

std::optional<PatternFile> HeuristicSourceToPatternFile(
    HeuristicSource source) {
  switch (source) {
    case HeuristicSource::kRegexes:
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
      return PatternFile::kLegacy;
#else
      return PatternFile::kDefault;
#endif
    case HeuristicSource::kAutofillMachineLearning:
    case HeuristicSource::kPasswordManagerMachineLearning:
      return std::nullopt;
  }
  NOTREACHED();
}

}  // namespace autofill
