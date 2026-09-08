// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_RANKER_EXAMPLE_UTIL_H_
#define COMPONENTS_ASSIST_RANKER_RANKER_EXAMPLE_UTIL_H_

#include <string>

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

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_RANKER_EXAMPLE_UTIL_H_
