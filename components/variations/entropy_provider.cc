// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/entropy_provider.h"

#include <algorithm>
#include <limits>

#include "base/check_op.h"
#include "base/hash/sha1.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "components/variations/variations_murmur_hash.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace variations {

SHA1EntropyProvider::SHA1EntropyProvider(const std::string& entropy_source)
    : entropy_source_(entropy_source) {
}

SHA1EntropyProvider::~SHA1EntropyProvider() {
}

double SHA1EntropyProvider::GetEntropyForTrial(
    base::StringPiece trial_name,
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

  unsigned char sha1_hash[base::kSHA1Length];
  base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(input.c_str()),
                      input.size(),
                      sha1_hash);

  uint64_t bits;
  static_assert(sizeof(bits) < sizeof(sha1_hash), "more data required");
  memcpy(&bits, sha1_hash, sizeof(bits));
  bits = base::ByteSwapToLE64(bits);

  return base::BitsToOpenEndedUnitInterval(bits);
}

NormalizedMurmurHashEntropyProvider::NormalizedMurmurHashEntropyProvider(
    ValueInRange entropy_value)
    : entropy_value_(entropy_value) {
  DCHECK_LT(entropy_value.value, entropy_value.range);
  DCHECK_LE(entropy_value.range, std::numeric_limits<uint16_t>::max());
}

NormalizedMurmurHashEntropyProvider::~NormalizedMurmurHashEntropyProvider() {}

double NormalizedMurmurHashEntropyProvider::GetEntropyForTrial(
    base::StringPiece trial_name,
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
    base::StringPiece trial_name,
    uint32_t randomization_seed) const {
  return base::RandDouble();
}

EntropyProviders::EntropyProviders(const std::string& high_entropy_value,
                                   ValueInRange low_entropy_value,
                                   bool enable_benchmarking)
    : low_entropy_(low_entropy_value),
      benchmarking_enabled_(enable_benchmarking) {
  if (!high_entropy_value.empty()) {
    high_entropy_.emplace(high_entropy_value);
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

}  // namespace variations
