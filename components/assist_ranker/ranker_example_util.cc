// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/ranker_example_util.h"

#include <math.h>

#include "base/bit_cast.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"

namespace assist_ranker {
namespace {
const uint64_t MASK32Bits = (1LL << 32) - 1;
constexpr int kFloatMainDigits = 23;
// Returns lower 32 bits of the hash of the input.
int32_t StringToIntBits(const std::string& str) {
  return base::HashMetricName(str) & MASK32Bits;
}

// Converts float to int32
int32_t FloatToIntBits(float f) {
  if (std::numeric_limits<float>::is_iec559) {
    // Directly bit_cast if float follows ieee754 standard.
    return base::bit_cast<int32_t>(f);
  } else {
    // Otherwise, manually calculate sign, exp and mantissa.
    // For sign.
    const uint32_t sign = f < 0;

    // For exponent.
    int exp;
    f = std::abs(std::frexp(f, &exp));
    // Add 126 to get non-negative format of exp.
    // This should not be 127 because the return of frexp is different from
    // ieee754 with a multiple of 2.
    const uint32_t exp_u = exp + 126;

    // Get mantissa.
    const uint32_t mantissa = std::ldexp(f * 2.0f - 1.0f, kFloatMainDigits);
    // Set each bits and return.
    return (sign << 31) | (exp_u << kFloatMainDigits) | mantissa;
  }
}

// Pair type, value and index into one int64.
int64_t PairInt(const uint64_t type,
                const uint32_t value,
                const uint64_t index) {
  return (type << 56) | (index << 32) | static_cast<uint64_t>(value);
}

}  // namespace

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

bool FeatureToInt64(const Feature& feature,
                    int64_t* const res,
                    const int index) {
  int32_t value = -1;
  int32_t type = feature.feature_type_case();
  switch (type) {
    case Feature::kBoolValue:
      value = static_cast<int32_t>(feature.bool_value());
      break;
    case Feature::kFloatValue:
      value = FloatToIntBits(feature.float_value());
      break;
    case Feature::kInt32Value:
      value = feature.int32_value();
      break;
    case Feature::kStringValue:
      value = StringToIntBits(feature.string_value());
      break;
    case Feature::kStringList:
      if (index >= 0 && index < feature.string_list().string_value_size()) {
        value = StringToIntBits(feature.string_list().string_value(index));
      } else {
        DVLOG(3) << "Invalid index for string list: " << index;
        NOTREACHED_IN_MIGRATION();
        return false;
      }
      break;
    default:
      DVLOG(3) << "Feature type is supported for logging: " << type;
      NOTREACHED_IN_MIGRATION();
      return false;
  }
  *res = PairInt(type, value, index);
  return true;
  }

bool GetOneHotValue(const std::string& key,
                    const RankerExample& example,
                    std::string* value) {
  Feature feature;
  if (!SafeGetFeature(key, example, &feature)) {
    return false;
  }
  if (feature.feature_type_case() != Feature::kStringValue) {
    DVLOG(1) << "Feature " << key
             << " exists, but is not the right type (Expected: "
             << Feature::kStringValue
             << " vs. Actual: " << feature.feature_type_case() << ")";
    return false;
  }
  *value = feature.string_value();
  return true;
}

// Converts string to a hex hash string.
std::string HashFeatureName(const std::string& feature_name) {
  uint64_t feature_key = base::HashMetricName(feature_name);
  return base::StringPrintf("%016" PRIx64, feature_key);
}

RankerExample HashExampleFeatureNames(const RankerExample& example) {
  RankerExample hashed_example;
  auto& output_features = *hashed_example.mutable_features();
  for (const auto& feature : example.features()) {
    output_features[HashFeatureName(feature.first)] = feature.second;
  }
  *hashed_example.mutable_target() = example.target();
  return hashed_example;
}

}  // namespace assist_ranker
