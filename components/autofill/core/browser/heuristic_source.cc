// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/heuristic_source.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

HeuristicSource GetActiveHeuristicSource() {
  if (base::FeatureList::IsEnabled(features::kAutofillModelPredictions) &&
      features::kAutofillModelPredictionsAreActive.Get()) {
    return HeuristicSource::kMachineLearning;
  }
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  return HeuristicSource::kLegacy;
#else
  if (!base::FeatureList::IsEnabled(
          features::kAutofillParsingPatternProvider)) {
    return HeuristicSource::kLegacy;
  }
  const std::string& source =
      features::kAutofillParsingPatternActiveSource.Get();
  CHECK(source == "default" || source == "experimental" ||
        source == "nextgen" || source == "legacy");
  return source == "default"        ? HeuristicSource::kDefault
         : source == "experimental" ? HeuristicSource::kExperimental
         : source == "nextgen"      ? HeuristicSource::kNextGen
         : source == "legacy"       ? HeuristicSource::kLegacy
                                    : HeuristicSource::kDefault;
#endif
}

DenseSet<HeuristicSource> GetNonActiveHeuristicSources() {
  DenseSet<HeuristicSource> sources{HeuristicSource::kLegacy};
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  if (base::FeatureList::IsEnabled(features::kAutofillParsingPatternProvider)) {
    sources.insert_all({HeuristicSource::kDefault,
                        HeuristicSource::kExperimental,
                        HeuristicSource::kNextGen});
  }
#endif
  if (base::FeatureList::IsEnabled(features::kAutofillModelPredictions)) {
    sources.insert(HeuristicSource::kMachineLearning);
  }
  sources.erase(GetActiveHeuristicSource());
  return sources;
}

absl::optional<PatternSource> HeuristicSourceToPatternSource(
    HeuristicSource source) {
  switch (source) {
    case HeuristicSource::kLegacy:
      return PatternSource::kLegacy;
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    case HeuristicSource::kDefault:
      return PatternSource::kDefault;
    case HeuristicSource::kExperimental:
      return PatternSource::kExperimental;
    case HeuristicSource::kNextGen:
      return PatternSource::kNextGen;
#endif
    case autofill::HeuristicSource::kMachineLearning:
      return absl::nullopt;
  }
  NOTREACHED_NORETURN();
}

HeuristicSource PatternSourceToHeuristicSource(PatternSource source) {
  switch (source) {
    case PatternSource::kLegacy:
      return HeuristicSource::kLegacy;
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    case PatternSource::kDefault:
      return HeuristicSource::kDefault;
    case PatternSource::kExperimental:
      return HeuristicSource::kExperimental;
    case PatternSource::kNextGen:
      return HeuristicSource::kNextGen;
#endif
  }
  NOTREACHED_NORETURN();
}
}  // namespace autofill
