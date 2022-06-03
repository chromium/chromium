// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/sim_hash.h"

#include "base/hash/legacy_hash.h"

#include <algorithm>
#include <cmath>

namespace federated_learning {

namespace {

uint64_t g_seed1 = 1ULL;
uint64_t g_seed2 = 2ULL;
constexpr double kTwoPi = 2.0 * 3.141592653589793;

// Hashes i and j to create a uniform RV in [0,1].
double RandomUniform(uint64_t i, uint64_t j, uint64_t seed) {
  uint64_t arr[2] = {i, j};
  uint64_t hashed = base::legacy::CityHash64WithSeed(
      base::as_bytes(
          base::make_span(reinterpret_cast<const char*>(arr), sizeof(arr))),
      seed);

  return static_cast<double>(hashed) /
         static_cast<double>(std::numeric_limits<uint64_t>::max());
}

// Uses the Box-Muller transform to generate a Gaussian from two uniform RVs in
// [0,1] derived from i and j.
double RandomGaussian(uint64_t i, uint64_t j) {
  double rv1 = RandomUniform(i, j, g_seed1);
  double rv2 = RandomUniform(j, i, g_seed2);

  DCHECK_LE(rv1, 1);
  DCHECK_GE(rv1, 0);
  DCHECK_LE(rv2, 1);
  DCHECK_GE(rv2, 0);

  // BoxMuller
  return std::sqrt(-2.0 * std::log(rv1)) * std::cos(kTwoPi * rv2);
}

}  // namespace

void SetSeedsForTesting(uint64_t seed1, uint64_t seed2) {
  g_seed1 = seed1;
  g_seed2 = seed2;
}

uint64_t SimHashWeightedFeatures(const WeightedFeatures& features,
                                 uint8_t output_dimensions) {
  DCHECK_LT(0u, output_dimensions);
  DCHECK_LE(output_dimensions, 64u);

  uint64_t result = 0;
  for (uint8_t d = 0; d < output_dimensions; ++d) {
    double acc = 0;

    for (const auto& feature_weight_pair : features) {
      acc += RandomGaussian(d, feature_weight_pair.first) *
             feature_weight_pair.second;
    }

    if (acc > 0)
      result |= (1ULL << d);
  }

  return result;
}

uint64_t SimHashStrings(const std::unordered_set<std::string>& input,
                        uint8_t output_dimensions) {
  DCHECK_LT(0u, output_dimensions);
  DCHECK_LE(output_dimensions, 64u);

  WeightedFeatures features;

  for (const std::string& s : input) {
    FeatureEncoding string_hash =
        base::legacy::CityHash64(base::as_bytes(base::make_span(s)));
    features.emplace(string_hash, FeatureWeight(1));
  }

  return SimHashWeightedFeatures(features, output_dimensions);
}

}  // namespace federated_learning
