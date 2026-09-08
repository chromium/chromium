// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/ranker_example_util.h"

namespace assist_ranker {

bool SafeGetFeature(const std::string& key,
                    const RankerExample& example,
                    Feature* feature) {
  auto p_feature = example.features().find(key);
  if (p_feature != example.features().end()) {
    if (feature)
      *feature = p_feature->second;
    return true;
  }
  return false;
}

bool GetFeatureValueAsFloat(const std::string& key,
                            const RankerExample& example,
                            float* value) {
  Feature feature;
  if (!SafeGetFeature(key, example, &feature)) {
    return false;
  }
  switch (feature.feature_type_case()) {
    case Feature::kBoolValue:
      *value = static_cast<float>(feature.bool_value());
      break;
    case Feature::kInt32Value:
      *value = static_cast<float>(feature.int32_value());
      break;
    case Feature::kFloatValue:
      *value = feature.float_value();
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace assist_ranker
