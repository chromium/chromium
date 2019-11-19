// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/entropy_provider.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "components/variations/hashing.h"
#include "components/variations/variations_murmur_hash.h"

namespace variations {

SHA1EntropyProvider::SHA1EntropyProvider(const std::string& entropy_source)
    : entropy_source_(entropy_source) {
}

SHA1EntropyProvider::~SHA1EntropyProvider() {
}

double SHA1EntropyProvider::GetEntropyForTrial(
    const std::string& trial_name,
    uint32_t randomization_seed) const {
  // Given enough input entropy, SHA-1 will produce a uniformly random spread
  // in its output space. In this case, the input entropy that is used is the
  // combination of the original |entropy_source_| and the |trial_name|.
  //
  // Note: If |entropy_source_| has very low entropy, such as 13 bits or less,
  // it has been observed that this method does not result in a uniform
  // distribution given the same |trial_name|. When using such a low entropy
  // source, NormalizedMurmurHashEntropyProvider should be used instead.
  std::string input(entropy_source_);
  input.append(randomization_seed == 0
                   ? trial_name
                   : base::NumberToString(randomization_seed));

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
    uint16_t low_entropy_source,
    size_t low_entropy_source_max)
    : low_entropy_source_(low_entropy_source),
      low_entropy_source_max_(low_entropy_source_max) {
  DCHECK_LT(low_entropy_source, low_entropy_source_max);
  DCHECK_LE(low_entropy_source_max, std::numeric_limits<uint16_t>::max());
}

NormalizedMurmurHashEntropyProvider::~NormalizedMurmurHashEntropyProvider() {}

double NormalizedMurmurHashEntropyProvider::GetEntropyForTrial(
    const std::string& trial_name,
    uint32_t randomization_seed) const {
  if (randomization_seed == 0) {
    randomization_seed = internal::VariationsMurmurHash::Hash(
        internal::VariationsMurmurHash::StringToLE32(trial_name),
        trial_name.length());
  }

  uint32_t x = internal::VariationsMurmurHash::Hash16(randomization_seed,
                                                      low_entropy_source_);
  int x_ordinal = 0;
  for (uint32_t i = 0; i < low_entropy_source_max_; i++) {
    uint32_t y = internal::VariationsMurmurHash::Hash16(randomization_seed, i);
    x_ordinal += (y < x);
  }

  DCHECK_GE(x_ordinal, 0);
  // There must have been at least one iteration where |x| == |y|, because
  // |i| == |low_entropy_source_|, and |x_ordinal| was not incremented in that
  // iteration, so |x_ordinal| < |low_entropy_source_max_|.
  DCHECK_LT(static_cast<size_t>(x_ordinal), low_entropy_source_max_);

  return static_cast<double>(x_ordinal) / low_entropy_source_max_;
}

}  // namespace variations
