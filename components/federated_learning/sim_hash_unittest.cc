// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/sim_hash.h"

#include <cmath>

#include "base/check_op.h"
#include "base/hash/legacy_hash.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace federated_learning {

namespace {

// Sparse representation of a 2^64 bit vector. Each number in the set represents
// the position of a bit that is being set.
using LargeBitVector = std::set<uint64_t>;

// Roll our own random uint64_t generator as we want deterministic outcome
// across different test runs.
uint64_t RandUint64() {
  static uint64_t seed = 0;
  ++seed;
  uint64_t arr[2] = {1234ULL, 4321ULL};
  return base::legacy::CityHash64WithSeed(
      base::as_bytes(
          base::make_span(reinterpret_cast<const char*>(arr), sizeof(arr))),
      seed);
}

// Inclusive [min, max]
uint64_t RandUint64InRange(uint64_t min, uint64_t max) {
  DCHECK_LE(min, max);
  if (min == 0ULL && max == std::numeric_limits<uint64_t>::max())
    return RandUint64();

  uint64_t range = max - min + 1;
  DCHECK_LE(2ULL, range);

  // We must discard random results above this number, as they would
  // make the random generator non-uniform.
  uint64_t max_acceptable_value =
      (std::numeric_limits<uint64_t>::max() / range) * range - 1;

  uint64_t rand_value;
  do {
    rand_value = RandUint64();
  } while (rand_value > max_acceptable_value);

  uint64_t result = (rand_value % range) + min;

  DCHECK_GE(result, min);
  DCHECK_LE(result, max);
  return result;
}

LargeBitVector RandLargeBitVector(
    size_t number_of_bits_set,
    uint64_t max_bit_position = std::numeric_limits<uint64_t>::max()) {
  LargeBitVector result;
  while (result.size() < number_of_bits_set) {
    result.insert(RandUint64InRange(0ULL, max_bit_position));
  }
  return result;
}

float DotProduct(const LargeBitVector& v1, const LargeBitVector& v2) {
  float result = 0;
  for (uint64_t pos : v1) {
    if (v2.count(pos))
      result += 1;
  }
  return result;
}

float Norm(const LargeBitVector& v) {
  return std::sqrt(DotProduct(v, v));
}

WeightedFeatures ToWeightedFeatures(const LargeBitVector& v) {
  WeightedFeatures result;
  for (FeatureEncoding feature : v) {
    result.emplace(feature, FeatureWeight(1));
  }
  return result;
}

size_t MultipleSimHashGetNumOutputBitsEqual(size_t repeat_times,
                                            uint8_t dimensions,
                                            const WeightedFeatures& f1,
                                            const WeightedFeatures& f2) {
  uint64_t seed1 = 1;
  uint64_t seed2 = 100000;

  uint64_t num_output_bits_equal = 0;
  for (size_t i = 0; i < repeat_times; ++i) {
    SetSeedsForTesting(seed1++, seed2++);

    uint64_t o1 = SimHashWeightedFeatures(f1, dimensions);
    uint64_t o2 = SimHashWeightedFeatures(f2, dimensions);
    for (uint8_t j = 0; j < dimensions; ++j) {
      if ((o1 & 1) == (o2 & 1))
        ++num_output_bits_equal;
      o1 >>= 1;
      o2 >>= 1;
    }
  }
  return num_output_bits_equal;
}

TEST(SimHashTest, HashValue) {
  WeightedFeatures empty;
  EXPECT_EQ(SimHashWeightedFeatures(empty, 1u), 0ULL);
  EXPECT_EQ(SimHashWeightedFeatures(empty, 16u), 0ULL);

  WeightedFeatures f0;
  f0.emplace(FeatureEncoding(0), FeatureWeight(1));
  EXPECT_EQ(SimHashWeightedFeatures(f0, 1u), 0ULL);
  EXPECT_EQ(SimHashWeightedFeatures(f0, 16u), 8632ULL);

  WeightedFeatures f1;
  f1.emplace(FeatureEncoding(0), FeatureWeight(123));
  EXPECT_EQ(SimHashWeightedFeatures(f1, 1u), 0ULL);
  EXPECT_EQ(SimHashWeightedFeatures(f1, 16u), 8632ULL);

  WeightedFeatures f2;
  f2.emplace(FeatureEncoding(999999), FeatureWeight(1));
  EXPECT_EQ(SimHashWeightedFeatures(f2, 1u), 1ULL);
  EXPECT_EQ(SimHashWeightedFeatures(f2, 16u), 28603ULL);

  WeightedFeatures f3;
  f3.emplace(FeatureEncoding(0), FeatureWeight(1));
  f3.emplace(FeatureEncoding(1), FeatureWeight(1));
  EXPECT_EQ(SimHashWeightedFeatures(f3, 1u), 0ULL);
  EXPECT_EQ(SimHashWeightedFeatures(f3, 16u), 10682ULL);

  WeightedFeatures f4;
  f4.emplace(FeatureEncoding(0), FeatureWeight(2));
  f4.emplace(FeatureEncoding(1), FeatureWeight(3));
  EXPECT_EQ(SimHashWeightedFeatures(f4, 1u), 0ULL);
  EXPECT_EQ(SimHashWeightedFeatures(f4, 16u), 2490ULL);
}

// Test that given random equally weighted input features, the chances of each
// bit in the SimHash result to be 0 and 1 are equally likely.
TEST(SimHashTest, ExpectationOnRandomUniformInput) {
  const uint8_t dimensions = 16u;
  const size_t repeat_times = 10000u;

  uint64_t totals[dimensions] = {0.0};

  for (size_t i = 0; i < repeat_times; ++i) {
    LargeBitVector v = RandLargeBitVector(RandUint64InRange(1u, 10u));
    uint64_t hash_result =
        SimHashWeightedFeatures(ToWeightedFeatures(v), dimensions);

    for (uint8_t j = 0; j < dimensions; ++j) {
      totals[j] += (hash_result & 1);
      hash_result >>= 1;
    }
  }

  const double expectation = 0.5;
  const double err_tolerance = 0.03;

  for (uint8_t j = 0; j < dimensions; ++j) {
    double avg = 1.0 * totals[j] / repeat_times;
    EXPECT_LT(avg, expectation + err_tolerance);
    EXPECT_GT(avg, expectation - err_tolerance);
  }
}

// Test that the cosine similarity is preserved.
TEST(SimHashTest, CosineSimilarity_NonOrthogonalInput) {
  const float kPi = 3.141592653589793;
  const uint8_t dimensions = 50u;
  const size_t repeat_times = 100u;

  // Generate v1 and v2 that are likely non-orthogonal.
  LargeBitVector v1 = RandLargeBitVector(4000u, 10000);
  LargeBitVector v2 = RandLargeBitVector(5000u, 10000);

  size_t num_output_bits_equal = MultipleSimHashGetNumOutputBitsEqual(
      repeat_times, dimensions, ToWeightedFeatures(v1), ToWeightedFeatures(v2));
  float avg = 1.0 * num_output_bits_equal / dimensions / repeat_times;
  float expectation =
      1.0 - std::acos(DotProduct(v1, v2) / (Norm(v1) * Norm(v2))) / kPi;

  // Verify that the expectation is different from 0.5 and 1 to make sure the
  // test is non-trivial.
  EXPECT_LT(expectation, 0.7);
  EXPECT_GT(expectation, 0.6);

  // Verify that SimHash(v1) and SimHash(v2) have approximately |expectation|
  // fraction of their bits in common.
  const double err_tolerance = 0.03;
  EXPECT_LT(avg, expectation + err_tolerance);
  EXPECT_GT(avg, expectation - err_tolerance);
}

// Test that when input v1 and v2 are orthogonal, SimHash(v1) and SimHash(v2)
// have approximately half their bits in common.
TEST(SimHashTest, CosineSimilarity_OrthogonalInput) {
  const uint8_t dimensions = 50u;
  const size_t repeat_times = 100u;

  // Generate v1 and v2 that are likely orthogonal
  LargeBitVector v1 = RandLargeBitVector(1u);
  LargeBitVector v2 = RandLargeBitVector(1u);

  size_t num_output_bits_equal = MultipleSimHashGetNumOutputBitsEqual(
      repeat_times, dimensions, ToWeightedFeatures(v1), ToWeightedFeatures(v2));
  float avg = 1.0 * num_output_bits_equal / dimensions / repeat_times;
  float expectation = 0.5;

  // Verify that SimHash(v1) and SimHash(v2) have approximately half their bits
  // in common.
  const double err_tolerance = 0.03;
  EXPECT_LT(avg, expectation + err_tolerance);
  EXPECT_GT(avg, expectation - err_tolerance);
}

}  // namespace

}  // namespace federated_learning
