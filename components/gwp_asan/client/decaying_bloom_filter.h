// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_DECAYING_BLOOM_FILTER_H_
#define COMPONENTS_GWP_ASAN_CLIENT_DECAYING_BLOOM_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <atomic>
#include <bit>
#include <limits>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"

namespace gwp_asan::internal {

// A lock-free counting Bloom filter that evicts keys after `kDecaySteps`
// calls to Add() using a ring buffer.
//
// Inspired by base::LockFreeBloomFilter, each key is hashed once with
// base::FastHash() and bit-sliced into `kNumHashes` indices. All state is held
// in relaxed atomics and is constexpr-constructible for safe global use.
template <size_t kNumCounters,
          size_t kNumHashes,
          size_t kDecaySteps,
          bool UseFakeHashFunctionsForTesting = false>
class DecayingBloomFilter {
 public:
  using Counter = uint16_t;

  static_assert(kNumCounters > 1 && std::has_single_bit(kNumCounters),
                "kNumCounters must be a power of two greater than 1");
  static_assert(std::has_single_bit(kDecaySteps),
                "kDecaySteps must be a power of two");
  static_assert(kNumHashes != 0, "kNumHashes must be non-zero");

  // To avoid computing multiple hashes, a single `kHashBits`-bit hash is
  // sliced into `kNumHashes` chunks of `kShiftWidth` bits each, masked with
  // `kCounterIndexMask` to index into `counters_`.
  static constexpr size_t kHashBits = std::numeric_limits<uint32_t>::digits;
  static constexpr size_t kCounterIndexMask = kNumCounters - 1;
  static constexpr size_t kShiftWidth = std::bit_width(kCounterIndexMask);
  static_assert(kNumHashes * kShiftWidth < kHashBits,
                "Must fit in uint32_t with room for kValidHashBit");

  // Bitmask for wrapping `ring_pos_` indices within `ring_`.
  static constexpr size_t kRingIndexMask = kDecaySteps - 1;

  static_assert(std::atomic<Counter>::is_always_lock_free);
  static_assert(std::atomic<uint32_t>::is_always_lock_free);
  static_assert(std::atomic<size_t>::is_always_lock_free);
  static_assert(std::numeric_limits<Counter>::max() >=
                kNumHashes * (kDecaySteps + 1));

  constexpr DecayingBloomFilter() = default;

  DecayingBloomFilter(const DecayingBloomFilter&) = delete;
  DecayingBloomFilter& operator=(const DecayingBloomFilter&) = delete;

  // Returns true if `key` may have been added within the last `kDecaySteps`
  // calls to Add().
  ALWAYS_INLINE bool Contains(uint64_t key) const {
    const uint32_t hash = ComputeHash(key);
    for (size_t i = 0; i < kNumHashes; ++i) {
      const size_t idx = GetSlotIndex(hash, i);
      if (counters_[idx].load(std::memory_order_relaxed) == 0) {
        return false;
      }
    }
    return true;
  }

  // Adds `key` and evicts the entry added `kDecaySteps` steps ago.
  void Add(uint64_t key) {
    const uint32_t hash = ComputeHash(key);
    IncrementCounters(hash);

    // `ring_pos_` is allowed to overflow. At its maximum value all bits are
    // ones so it maps to the last ring slot, and overflowing to zero seamlessly
    // wraps to slot zero. Bitwise AND is used instead of modulo for
    // performance.
    const size_t pos = ring_pos_.fetch_add(1u, std::memory_order_relaxed);
    const uint32_t evicted_hash =
        ring_[pos & kRingIndexMask].exchange(hash, std::memory_order_relaxed);
    if (evicted_hash != kEmptyRingSlot) {
      DecrementCounters(evicted_hash);
    }
  }

  size_t CountNonZeroCountersForTesting() const {
    size_t count = 0;
    for (const auto& counter : counters_) {
      if (counter.load(std::memory_order_relaxed) > 0) {
        ++count;
      }
    }
    return count;
  }

  Counter GetCounterForTesting(size_t index) const {
    return counters_[index].load(std::memory_order_relaxed);
  }

 private:
  // Zero marks an unused ring buffer slot so that `ring_` can be zero-
  // initialized in BSS. All valid hashes set `kValidHashBit` so they are
  // never zero, including for key = 0.
  static constexpr uint32_t kEmptyRingSlot = 0;
  static constexpr uint32_t kValidHashBit = uint32_t{1} << (kHashBits - 1);

  ALWAYS_INLINE static uint32_t ComputeHash(uint64_t key) {
    if constexpr (UseFakeHashFunctionsForTesting) {
      return static_cast<uint32_t>(key) | kValidHashBit;
    } else {
      return static_cast<uint32_t>(
                 base::FastHash(base::byte_span_from_ref(key))) |
             kValidHashBit;
    }
  }

  ALWAYS_INLINE static size_t GetSlotIndex(uint32_t hash, size_t hash_index) {
    return static_cast<size_t>((hash >> (hash_index * kShiftWidth)) &
                               kCounterIndexMask);
  }

  void IncrementCounters(uint32_t hash) {
    for (size_t i = 0; i < kNumHashes; ++i) {
      const size_t idx = GetSlotIndex(hash, i);
      counters_[idx].fetch_add(1u, std::memory_order_relaxed);
    }
  }

  void DecrementCounters(uint32_t hash) {
    for (size_t i = 0; i < kNumHashes; ++i) {
      const size_t idx = GetSlotIndex(hash, i);
      counters_[idx].fetch_sub(1u, std::memory_order_relaxed);
    }
  }

  std::array<std::atomic<Counter>, kNumCounters> counters_ = {};
  std::array<std::atomic<uint32_t>, kDecaySteps> ring_ = {};
  std::atomic<size_t> ring_pos_ = 0;
};

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_DECAYING_BLOOM_FILTER_H_
