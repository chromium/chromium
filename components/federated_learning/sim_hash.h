// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEDERATED_LEARNING_SIM_HASH_H_
#define COMPONENTS_FEDERATED_LEARNING_SIM_HASH_H_

#include <stdint.h>
#include <map>
#include <string>
#include <unordered_set>

namespace federated_learning {

using FeatureEncoding = uint64_t;
using FeatureWeight = int;
using WeightedFeatures = std::map<FeatureEncoding, FeatureWeight>;

// Set the two seeds used for generating the random gaussian.
void SetSeedsForTesting(uint64_t seed1, uint64_t seed2);

// SimHash a set of weighted features to an |output_dimensions| bit number.
// |output_dimensions| must be greater than 0 and no greater than 64.
uint64_t SimHashWeightedFeatures(const WeightedFeatures& features,
                                 uint8_t output_dimensions);

// SimHash a set of strings to an |output_dimensions| bit number.
// |output_dimensions| must be greater than 0 and no greater than 64.
uint64_t SimHashStrings(const std::unordered_set<std::string>& input,
                        uint8_t output_dimensions);

}  // namespace federated_learning

#endif  // COMPONENTS_FEDERATED_LEARNING_SIM_HASH_H_
