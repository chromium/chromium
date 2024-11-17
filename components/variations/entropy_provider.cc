// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/entropy_provider.h"

#include <algorithm>
#include <limits>
#include <string_view>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/numerics/byte_conversions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/variations/variations_murmur_hash.h"

namespace variations {

SHA1EntropyProvider::SHA1EntropyProvider(std::string_view entropy_source)
    : entropy_source_(entropy_source) {}

SHA1EntropyProvider::~SHA1EntropyProvider() = default;

double SHA1EntropyProvider::GetEntropyForTrial(
    std::string_view trial_name,
    uint32_t randomization_seed) const {
  // Given enough input entropy, SHA-1 will produce a uniformly random spread
  // in its output space. In this case, the input entropy that is used is the
  // combination of the original |entropy_source_| and the |trial_name|.
  //
  // Note: If |entropy_source_| has very low entropy, such as 13 bits or less,
  // it has been observed that this method does not result in a uniform
  // distribution given the same |trial_name|. When using such a low entropy
  // source, NormalizedMurmurHashEntropyProvider should be used instead.
  std::string input = base::StrCat(
      {entropy_source_, randomization_seed == 0
                            ? trial_name
                            : base::NumberToString(randomization_seed)});

  base::SHA1Digest sha1_hash = base::SHA1Hash(base::as_byte_span(input));
  uint64_t bits = base::U64FromLittleEndian(base::span(sha1_hash).first<8u>());

  return base::BitsToOpenEndedUnitInterval(bits);
}

NormalizedMurmurHashEntropyProvider::NormalizedMurmurHashEntropyProvider(
    ValueInRange entropy_value)
    : entropy_value_(entropy_value) {
  DCHECK_LT(entropy_value.value, entropy_value.range);
  DCHECK_LE(entropy_value.range, std::numeric_limits<uint16_t>::max());
}

NormalizedMurmurHashEntropyProvider::~NormalizedMurmurHashEntropyProvider() =
    default;

double NormalizedMurmurHashEntropyProvider::GetEntropyForTrial(
    std::string_view trial_name,
    uint32_t randomization_seed) const {
  if (randomization_seed == 0) {
    randomization_seed = internal::VariationsMurmurHash::Hash(
        internal::VariationsMurmurHash::StringToLE32(trial_name),
        trial_name.length());
  }
  uint32_t x = internal::VariationsMurmurHash::Hash16(randomization_seed,
                                                      entropy_value_.value);
  int x_ordinal = 0;
  for (uint32_t i = 0; i < entropy_value_.range; i++) {
    uint32_t y = internal::VariationsMurmurHash::Hash16(randomization_seed, i);
    x_ordinal += (y < x);
  }

  DCHECK_GE(x_ordinal, 0);
  // There must have been at least one iteration where |x| == |y|, because
  // |i| == |entropy_value_|, and |x_ordinal| was not incremented in that
  // iteration, so |x_ordinal| < |entropy_domain_|.
  DCHECK_LT(static_cast<uint32_t>(x_ordinal), entropy_value_.range);
  return static_cast<double>(x_ordinal) / entropy_value_.range;
}

SessionEntropyProvider::~SessionEntropyProvider() = default;

double SessionEntropyProvider::GetEntropyForTrial(
    std::string_view trial_name,
    uint32_t randomization_seed) const {
  return base::RandDouble();
}

EntropyProviders::EntropyProviders(std::string_view high_entropy_value,
                                   ValueInRange low_entropy_value,
                                   std::string_view limited_entropy_value,
                                   bool enable_benchmarking)
    : low_entropy_(low_entropy_value),
      benchmarking_enabled_(enable_benchmarking) {
  if (!high_entropy_value.empty()) {
    high_entropy_.emplace(high_entropy_value);
  }
  if (!limited_entropy_value.empty()) {
    limited_entropy_.emplace(limited_entropy_value);
  }
}

EntropyProviders::~EntropyProviders() = default;

const base::FieldTrial::EntropyProvider& EntropyProviders::default_entropy()
    const {
  if (high_entropy_.has_value())
    return high_entropy_.value();
  return low_entropy_;
}

const base::FieldTrial::EntropyProvider& EntropyProviders::low_entropy() const {
  return low_entropy_;
}

const base::FieldTrial::EntropyProvider& EntropyProviders::session_entropy()
    const {
  return session_entropy_;
}

const base::FieldTrial::EntropyProvider& EntropyProviders::limited_entropy()
    const {
  // The caller must initialize the instance with a
  // |limited_entropy_randomization_source|.
  CHECK(has_limited_entropy());
  return limited_entropy_.value();
}

}  // namespace variations
