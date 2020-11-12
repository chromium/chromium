// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/pattern_provider/test_pattern_provider.h"

#include "base/feature_list.h"
#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

TestPatternProvider::TestPatternProvider() {
  // TODO(crbug/1147608) This is an ugly hack to avoid loading the JSON. The
  // motivation is that some Android unit tests fail because a dependency is
  // missing. Instead of fixing this dependency, we'll go for an alternative
  // solution that avoids the whole async/sync problem.
  if (base::FeatureList::IsEnabled(
          features::kAutofillUsePageLanguageToSelectFieldParsingPatterns) ||
      base::FeatureList::IsEnabled(
          features::
              kAutofillApplyNegativePatternsForFieldTypeDetectionHeuristics)) {
    base::Optional<PatternProvider::Map> patterns =
        field_type_parsing::GetPatternsFromResourceBundleSynchronously();
    if (patterns)
      SetPatterns(patterns.value(), base::Version(), true);

    PatternProvider::SetPatternProviderForTesting(this);
  }
}

TestPatternProvider::~TestPatternProvider() {
  PatternProvider::ResetPatternProvider();
}

}  // namespace autofill
