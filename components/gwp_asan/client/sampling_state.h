// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_STATE_H_
#define COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_STATE_H_

#include <stddef.h>  // for size_t

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <random>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "components/gwp_asan/client/decaying_bloom_filter.h"
#include "components/gwp_asan/client/thread_local_random_bit_generator.h"
#include "components/gwp_asan/client/thread_local_state.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace gwp_asan::internal {

enum ParentAllocator {
  MALLOC = 0,
  PARTITIONALLOC = 1,
  LIGHTWEIGHTDETECTOR = 2,
  EXTREMELIGHTWEIGHTDETECTOR = 3,
};

// Class that encapsulates the current sampling state. Sampling is performed
// using a counter stored in thread-local storage.
//
// This class is templated so that a thread-local global it contains is not
// shared between different instances (used by shims for different allocators.)
template <ParentAllocator PA>
class SamplingState : ThreadLocalState<SamplingState<PA>> {
 public:
  using TLS = ThreadLocalState<SamplingState<PA>>;

  constexpr SamplingState() = default;

  void Init(size_t sampling_frequency) {
    DCHECK_GT(sampling_frequency, 0U);
    InitWithProbability(1.0 / sampling_frequency);
  }

  void InitWithProbability(double sampling_probability) {
    DCHECK_GT(sampling_probability, 0.0);
    DCHECK_LE(sampling_probability, 1.0);
    sampling_probability_ = sampling_probability;

    ThreadLocalRandomBitGenerator::InitIfNeeded();
    TLS::InitIfNeeded();
  }

  void SetSampleSizeRestriction(size_t sampling_min_size,
                                size_t sampling_max_size) {
    sampling_min_size_ = sampling_min_size;
    sampling_max_size_ = sampling_max_size;
  }

  // Return true if this allocation should be sampled.
  ALWAYS_INLINE bool Sample(size_t alloc_size = 0) {
    // For a new thread the initial TLS value will be zero, we do not want to
    // sample on zero as it will always sample the first allocation on thread
    // creation and heavily bias allocations towards that particular call site.
    //
    // Instead, use zero to mean 'get a new counter value' and one to mean
    // that this allocation should be sampled.
    if (alloc_size != 0 &&
        (alloc_size < sampling_min_size_ || sampling_max_size_ < alloc_size)) {
      // Skip sampling to increase chance of catching an OOB issue
      return false;
    }

    size_t samples_left = TLS::GetState();
    if (samples_left == 0) [[unlikely]] {
      samples_left = NextSample();
    }

    --samples_left;
    TLS::SetState(samples_left);
    return samples_left == 0;
  }

 private:
  // Sample an allocation on every average one out of every
  // |sampling_frequency_| allocations.
  size_t NextSample() {
    ThreadLocalRandomBitGenerator generator;
    std::geometric_distribution<size_t> distribution(sampling_probability_);
    return distribution(generator) + 1;
  }

  size_t sampling_min_size_ = 0;
  size_t sampling_max_size_ = std::numeric_limits<size_t>::max();
  double sampling_probability_ = 0;
};

// Parameters for decaying bloom filter to track recently sampled allocation
// types.
inline constexpr size_t kSmartSamplingNumCounters = 8192;
inline constexpr size_t kSmartSamplingNumHashes = 2;
inline constexpr size_t kSmartSamplingDecaySteps = 1024;

// Number of samples just below the baseline reserved for unseen types.
inline constexpr size_t kSmartSamplingReservedForUnseenTypes = 16;

// Extends `SamplingState<PA>` with a decaying Bloom filter to prioritize
// sampling unseen allocation types. `key` is an integer identifying the
// allocation type (e.g., `allocator_shim::AllocToken`).
//
// Shares the TLS slot of `SamplingState<PA>` and keeps its own state in static
// members, so use a single instance per `PA` in place of `SamplingState<PA>`.
template <ParentAllocator PA>
class SmartSamplingState : public SamplingState<PA> {
 public:
  constexpr SmartSamplingState() = default;

  void Init(size_t sampling_frequency,
            bool enable_smart_sampling,
            size_t rate_multiplier) {
    CHECK_GE(rate_multiplier, 1U);
    smart_sampling_enabled_ = enable_smart_sampling;
    rate_multiplier_ = rate_multiplier;
    if (enable_smart_sampling) {
      DCHECK_GT(sampling_frequency, 0U);
      SamplingState<PA>::InitWithProbability(std::min(
          1.0, static_cast<double>(rate_multiplier) / sampling_frequency));
    } else {
      SamplingState<PA>::Init(sampling_frequency);
    }
  }

  void ResetForTesting() {
    SamplingState<PA>::TLS::InitIfNeeded();
    SamplingState<PA>::TLS::SetState(0);
    smart_sampling_enabled_ = false;
    rate_multiplier_ = 1;
    candidates_.store(0, std::memory_order_relaxed);
    accepted_.store(0, std::memory_order_relaxed);
    filter_.ResetForTesting();
  }

  ALWAYS_INLINE bool Sample(size_t alloc_size, size_t key) {
    if (!SamplingState<PA>::Sample(alloc_size)) {
      return false;
    }
    if (smart_sampling_enabled_) {
      return EvaluateCandidate(key);
    }
    return true;
  }

 private:
  using Filter = DecayingBloomFilter<kSmartSamplingNumCounters,
                                     kSmartSamplingNumHashes,
                                     kSmartSamplingDecaySteps>;

  NOINLINE static bool EvaluateCandidate(size_t key) {
    const uint64_t candidates =
        candidates_.fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t accepted = accepted_.load(std::memory_order_relaxed);

    bool accept;
    if ((accepted + kSmartSamplingReservedForUnseenTypes) * rate_multiplier_ <
        candidates) {
      // Accept even seen types.
      filter_.Add(key);
      accept = true;
    } else if (accepted * rate_multiplier_ < candidates) {
      // Accept only unseen types.
      accept = filter_.AddIfNotContained(key);
    } else {
      // Reject any type.
      accept = false;
    }

    if (accept) {
      accepted_.fetch_add(1, std::memory_order_relaxed);
    }
    return accept;
  }

  static inline bool smart_sampling_enabled_ = false;
  static inline size_t rate_multiplier_ = 1;

  // Static so that it is placed in .bss.
  static inline Filter filter_;
  static inline std::atomic<uint64_t> candidates_ = 0;
  static inline std::atomic<uint64_t> accepted_ = 0;
};

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_STATE_H_
