// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_HEURISTIC_SOURCE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_HEURISTIC_SOURCE_H_

#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

// The different sets of heuristic sources that are available.
// Local heuristics can either be regex or machine learning model.
// At field level, heuristic predictions are associated to a HeuristicSource,
// describing which mechanism computed them.
// At parsing level, only the set of regexes used is relevant. There, the code
// is parameterized by a PatternSource (a subset of HeuristicSource).
// When adding a new value to PatternSource, add it to HeuristicSource
// as well. When adding a new value to HeuristicSource, it's required to account
// for it in `GetActiveHeuristicSource()`, `GetNonActiveHeuristicSources()`
// and `HeuristicToPatternSource()`.
enum class HeuristicSource {
  // Same values used in `PatternSource` with additional value
  // for the ML model.
  kLegacy,
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  kDefault,
  kExperimental,
  kNextGen,
#endif
  kMachineLearning,
  kMaxValue = kMachineLearning
};

// The active and non active heuristic sources depend on the build
// config and Finch configs. In the unlikely case that
// `kAutofillModelPredictions` and `kAutofillParsingPatternProvider`
// are both enabled, `kAutofillModelPredictions` is prioritized.
HeuristicSource GetActiveHeuristicSource();
DenseSet<HeuristicSource> GetNonActiveHeuristicSources();

// Converts a `HeuristicSource` to `PatternSource`. If the passed
// source is not a `PatternSource` then a nullopt is returned.
absl::optional<PatternSource> HeuristicSourceToPatternSource(
    HeuristicSource source);
HeuristicSource PatternSourceToHeuristicSource(PatternSource source);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_HEURISTIC_SOURCE_H_
