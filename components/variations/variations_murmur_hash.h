// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_MURMUR_HASH_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_MURMUR_HASH_H_

#include <cstdint>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"

namespace variations {
namespace internal {

// Hash utilities for NormalizedMurmurHashEntropyProvider. For more info, see:
// https://docs.google.com/document/d/1cPF5PruriWNP2Z5gSkq4MBTm0wSZqLyIJkUO9ekibeo
class COMPONENT_EXPORT(VARIATIONS) VariationsMurmurHash {
 public:
  // Prepares data to be hashed by VariationsMurmurHash: align and zero-pad to a
  // multiple of 4 bytes, and produce the same uint32_t values regardless of
  // platform endianness. ("abcd" will always become 0x64636261). Any padding
  // will appear in the more-significant bytes of the last uint32_t.
  static std::vector<uint32_t> StringToLE32(std::string_view data);

  // Hash is a reimplementation of MurmurHash3_x86_32 from third_party/smhasher/
  // which works on all architectures. MurmurHash3_x86_32 does unaligned reads
  // (not generally safe on ARM) if the input bytes start on an unaligned
  // address, and it assumes little-endianness. Hash produces the same result
  // for the same input uint32_t values, regardless of platform endianness, and
  // it produces the same results that MurmurHash3_x86_32 would produce on a
  // little-endian platform.
  //
  // |length| is the number of bytes to hash. It mustn't exceed data.size() * 4.
  // If length % 4 != 0, Hash will consume the less-significant bytes of the
  // last uint32_t first.
  //
  // MurmurHash3_x86_32 takes a seed, for which 0 is the typical value. Hash
  // hard-codes the seed to 0, since NormalizedMurmurHashEntropyProvider doesn't
  // use it.
  static uint32_t Hash(const std::vector<uint32_t>& data, size_t length);

  // A version of Hash which is specialized for exactly 2 bytes of data and
  // allows a nonzero seed. NormalizedMurmurHashEntropyProvider calls this in a
  // loop, |kMaxLowEntropySize| times per study, so it must be fast.
  ALWAYS_INLINE static uint32_t Hash16(uint32_t seed, uint16_t data) {
    uint32_t h1 = seed, k1 = data;

    // tail
    k1 *= c1;
    k1 = RotateLeft(k1, 15);
    k1 *= c2;
    h1 ^= k1;

    // finalization
    h1 ^= 2;
    h1 = FinalMix(h1);

    return h1;
  }

 private:
  static const uint32_t c1 = 0xcc9e2d51;
  static const uint32_t c2 = 0x1b873593;

  ALWAYS_INLINE static uint32_t RotateLeft(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
  }

  ALWAYS_INLINE static uint32_t FinalMix(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
  }
};

}  // namespace internal
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_MURMUR_HASH_H_
