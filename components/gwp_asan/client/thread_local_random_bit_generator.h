// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_THREAD_LOCAL_RANDOM_BIT_GENERATOR_H_
#define COMPONENTS_GWP_ASAN_CLIENT_THREAD_LOCAL_RANDOM_BIT_GENERATOR_H_

#include <limits>

#include "base/compiler_specific.h"
#include "base/rand_util.h"
#include "components/gwp_asan/client/thread_local_state.h"

namespace gwp_asan::internal {

namespace {
template <std::size_t N>
struct XorShiftImpl;

template <>
struct XorShiftImpl<8> {
  static uint64_t apply(uint64_t x) {
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return x;
  }
};

template <>
struct XorShiftImpl<4> {
  static uint32_t apply(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
  }
};

template <typename result_type>
result_type XorShift(result_type x) {
  static_assert(sizeof(result_type) == 4 || sizeof(result_type) == 8,
                "Unsupported size");
  return XorShiftImpl<sizeof(result_type)>::apply(x);
}
}  // namespace

// Efficient pseudo-random number generator (PRNG) tailored for Thread-Local
// Storage (TLS) slots. Chooses between XorShift 64 and XorShift 32 based on
// TLS slot size.
//
// Note: GWP-ASan previously relied on a high-entropy source with the intent of
// preventing attackers from identifying allocations originating from GWP-ASan.
// However, it seems like there are more practical methods for attackers to
// achieve this, bypassing the need to crack the PRNG state. Additionally, an
// attacker can simply neutralize GWP-ASan by making numerous small allocations,
// exhausting all available slots.
class ThreadLocalRandomBitGenerator
    : ThreadLocalState<ThreadLocalRandomBitGenerator> {
  using TLS = ThreadLocalState<ThreadLocalRandomBitGenerator>;

 public:
  using result_type = uintptr_t;
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() {
    return std::numeric_limits<result_type>::max();
  }

  // Classes that use this generator need to call this function first. Safe to
  // call multiple times.
  static void InitIfNeeded() { TLS::InitIfNeeded(); }

  result_type operator()() const {
    result_type x = static_cast<result_type>(TLS::GetState());
    if (!x) [[unlikely]] {
      std::uniform_int_distribution<result_type> distribution;
      base::NonAllocatingRandomBitGenerator generator;
      x = distribution(generator);
    }

    x = XorShift(x);
    TLS::SetState(x);
    return x;
  }

  ThreadLocalRandomBitGenerator() = default;
  ~ThreadLocalRandomBitGenerator() = default;
};

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_THREAD_LOCAL_RANDOM_BIT_GENERATOR_H_
