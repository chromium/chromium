// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_STATE_H_
#define COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_STATE_H_

#include <stddef.h>  // for size_t
#include <limits>
#include <random>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "components/gwp_asan/client/thread_local_random_bit_generator.h"
#include "components/gwp_asan/client/thread_local_state.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace gwp_asan {
namespace internal {

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
    sampling_probability_ = 1.0 / sampling_frequency;

    ThreadLocalRandomBitGenerator::InitIfNeeded();
    TLS::InitIfNeeded();
  }

  // Return true if this allocation should be sampled.
  ALWAYS_INLINE bool Sample() {
    // For a new thread the initial TLS value will be zero, we do not want to
    // sample on zero as it will always sample the first allocation on thread
    // creation and heavily bias allocations towards that particular call site.
    //
    // Instead, use zero to mean 'get a new counter value' and one to mean
    // that this allocation should be sampled.
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

  double sampling_probability_ = 0;
};

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_STATE_H_
