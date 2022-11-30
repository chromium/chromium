// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_RANKER_EXAMPLE_UTIL_H_
#define COMPONENTS_ASSIST_RANKER_RANKER_EXAMPLE_UTIL_H_

#include "components/assist_ranker/proto/ranker_example.pb.h"

namespace assist_ranker {

// If |key| feature is found in |example|, fills in |feature| and return true.
// Returns false if the feature is not found. |feature| can be nullptr. In such
// a case, the return value is not changed, but |feature| will not be filled in.
// This can be used to check for the presence of a key.
[[nodiscard]] bool SafeGetFeature(const std::string& key,
                                  const RankerExample& example,
                                  Feature* feature);

// Extract value from |feature| for scalar feature types. Returns true and fills
// in |value| if the feature is found and has a float, int32 or bool value.
// Returns false otherwise.
[[nodiscard]] bool GetFeatureValueAsFloat(const std::string& key,
                                          const RankerExample& example,
                                          float* value);

// Converts a Ranker Feature to an int64. For feature list, this converts the
// index-th value of the list.
// A feature is converted to an int64 by:
// (a) use low32 bits represent the value of the feature.
//     a.1) bool_value, int32_value is directly converted to an int32.
//     a.2) string_value is hashed to an int32.
//     a.3) float_value is directly bit_cast into int32 if it follows ieee754
//          standard; otherwise manually calculate sign, exponent and mantissa.
// (b) use high32 bits represent the type of the feature.
//     b.1) use high8 bits represent the feature_type_case.
//     b.2) use low24 bits represent the index if the feature is a list.
// Returns true if the feature is converted successfully; false otherwise.
bool FeatureToInt64(const Feature& feature, int64_t* res, int index = 0);

// Extract category from one-hot feature. Returns true and fills
// in |value| if the feature is found and is of type string_value. Returns false
// otherwise.
[[nodiscard]] bool GetOneHotValue(const std::string& key,
                                  const RankerExample& example,
                                  std::string* value);

// Converts a string to a hex ahsh string.
std::string HashFeatureName(const std::string& feature_name);

// Hashes feature names to an hex string.
// Features logged through UKM will apply this transformation when logging
// features, so models trained on UKM data are expected to have hashed input
// feature names.
RankerExample HashExampleFeatureNames(const RankerExample& example);

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_RANKER_EXAMPLE_UTIL_H_
