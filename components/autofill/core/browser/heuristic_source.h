// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_HEURISTIC_SOURCE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_HEURISTIC_SOURCE_H_

#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

// The different ways to compute heuristic predictions.
// At field level, heuristic predictions are associated to a HeuristicSource,
// describing which mechanism computed them.
enum class HeuristicSource {
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  kLegacyRegexes,
#else
  kDefaultRegexes,
  // Corresponds to regexes from the default file, but with at least one
  // `RegexFeature` enabled.
  kExperimentalRegexes,
  kPredictionImprovementRegexes,
#endif
  kMachineLearning,
  kMaxValue = kMachineLearning
};

// The active heuristic sources depend on the build config and Finch configs.
HeuristicSource GetActiveHeuristicSource();

// Converts a `HeuristicSource` the corresponding `PatternFile`, in case the
// `source` is using regexes. Otherwise, nullopt is returned.
std::optional<PatternFile> HeuristicSourceToPatternFile(HeuristicSource source);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_HEURISTIC_SOURCE_H_
