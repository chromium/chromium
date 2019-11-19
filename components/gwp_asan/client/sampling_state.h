// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_STATE_H_
#define COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_STATE_H_

#include <stddef.h>  // for size_t
#include <limits>
#include <random>

#include "base/compiler_specific.h"
#include "base/rand_util.h"
#include "build/build_config.h"

#if defined(OS_MACOSX) || defined(OS_ANDROID)
#define USE_PTHREAD_TLS
#include <pthread.h>
#endif

namespace gwp_asan {
namespace internal {

enum ParentAllocator {
  MALLOC = 0,
  PARTITIONALLOC = 1,
};

// Class that encapsulates the current sampling state. Sampling is performed
// using a counter stored in thread-local storage.
//
// This class is templated so that a thread-local global it contains is not
// shared between different instances (used by shims for different allocators.)
template <ParentAllocator PA>
class SamplingState {
 public:
  constexpr SamplingState() {}

  void Init(size_t sampling_frequency) {
    DCHECK_GT(sampling_frequency, 0U);
    sampling_probability_ = 1.0 / sampling_frequency;

#if defined(USE_PTHREAD_TLS)
    pthread_key_create(&tls_key_, nullptr);
#endif
  }

  // Return true if this allocation should be sampled.
  ALWAYS_INLINE bool Sample() {
    // For a new thread the initial TLS value will be zero, we do not want to
    // sample on zero as it will always sample the first allocation on thread
    // creation and heavily bias allocations towards that particular call site.
    //
    // Instead, use zero to mean 'get a new counter value' and one to mean
    // that this allocation should be sampled.
    size_t samples_left = GetCounter();
    if (UNLIKELY(!samples_left))
      samples_left = NextSample();

    SetCounter(samples_left - 1);
    return (samples_left == 1);
  }

 private:
  // Sample an allocation on every average one out of every
  // |sampling_frequency_| allocations.
  size_t NextSample() {
    base::RandomBitGenerator generator;
    std::geometric_distribution<size_t> distribution(sampling_probability_);
    return distribution(generator) + 1;
  }

#if !defined(USE_PTHREAD_TLS)
  ALWAYS_INLINE size_t GetCounter() { return tls_counter_; }
  ALWAYS_INLINE void SetCounter(size_t value) { tls_counter_ = value; }

  static thread_local size_t tls_counter_;
#else
  // On macOS and Android (before Q), the first use of a thread_local variable
  // on a new thread will cause an allocation, leading to infinite recursion.
  // Instead, use pthread TLS to store the counter.
  //
  // TODO: This is not necessary for PartitionAlloc and likely slower, refactor
  // SamplingState to be able to use pthread TLS for malloc() and thread_local
  // for PartitionAlloc in this case.
  ALWAYS_INLINE size_t GetCounter() {
    return reinterpret_cast<size_t>(pthread_getspecific(tls_key_));
  }

  ALWAYS_INLINE void SetCounter(size_t value) {
    pthread_setspecific(tls_key_, reinterpret_cast<void*>(value));
  }

  pthread_key_t tls_key_ = 0;
#endif

  double sampling_probability_ = 0;
};

#if !defined(USE_PTHREAD_TLS)
template <ParentAllocator PA>
thread_local size_t SamplingState<PA>::tls_counter_ = 0;
#endif

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_STATE_H_
