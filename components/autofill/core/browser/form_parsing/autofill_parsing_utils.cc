// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

namespace {

// Maps `RegexFeature`s to their corresponding `base::Feature`s.
const base::Feature* GetFeatureOfRegexFeature(RegexFeature feature) {
  switch (feature) {
    case RegexFeature::kUnusedDummyFeature:
      return nullptr;
    case RegexFeature::kAutofillGreekRegexes:
      return &features::kAutofillGreekRegexes;
  }
  NOTREACHED();
}

}  // namespace

DenseSet<RegexFeature> GetActiveRegexFeatures() {
  DenseSet<RegexFeature> active_features;
  for (RegexFeature regex_feature : DenseSet<RegexFeature>::all()) {
    const base::Feature* base_feature = GetFeatureOfRegexFeature(regex_feature);
    if (base_feature && base::FeatureList::IsEnabled(*base_feature)) {
      active_features.insert(regex_feature);
    }
  }
  return active_features;
}

bool MatchingPattern::IsActive(DenseSet<RegexFeature> active_features) const {
  return feature.has_value()
             ? active_features.contains(feature.feature()) == feature.enabled()
             : true;
}

}  // namespace autofill
