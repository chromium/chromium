// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_THREAD_LOCAL_STATE_H_
#define COMPONENTS_GWP_ASAN_CLIENT_THREAD_LOCAL_STATE_H_

#include "base/check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
// On macOS and Android (before Q), the first use of a `thread_local` variable
// on a new thread will cause an allocation, leading to infinite recursion.
// Also, `thread_local` goes through `emutls` on Android, which is slower than
// `pthread_getspecific`.
#define THREAD_LOCAL_STATE_USES_PARTITION_ALLOC_TLS
#elif BUILDFLAG(IS_POSIX) && defined(COMPONENT_BUILD)
// On POSIX platforms when built as component build, use of a `thread_local`
// variable may or may not cause a call to `free()` depending on an
// implementation of TLS. At least in case of glibc/glibc/elf/dl-tls.c,
// `_dl_update_slotinfo()` calls `free()` (as of Apr 2024).
//
// This is a problem for LUD (Lightweight UaF Detector) and ELUD (Extreme LUD)
// as they call `SamplingState::Sample()` inside `free()`, which leads to
// infinite recursion. (See also https://crbug.com/332234169)
//
// Fortunately, statically-linked executables (non-component builds) do not hit
// the problem because they use the "local-exec" model of TLS (i.e. do not call
// any library function).
#define THREAD_LOCAL_STATE_USES_PARTITION_ALLOC_TLS
#endif

#if defined(THREAD_LOCAL_STATE_USES_PARTITION_ALLOC_TLS)
#include "partition_alloc/partition_tls.h"
#endif

namespace gwp_asan::internal {

// Class that provides the optimal thread-local storage implementation depending
// on the platform.
template <typename T>
class ThreadLocalState {
 protected:
  static void InitIfNeeded() {
#if defined(THREAD_LOCAL_STATE_USES_PARTITION_ALLOC_TLS)
    if (!tls_key_) {
      partition_alloc::internal::PartitionTlsCreate(&tls_key_, nullptr);
    }
#endif
  }

#if !defined(THREAD_LOCAL_STATE_USES_PARTITION_ALLOC_TLS)
  ALWAYS_INLINE static uintptr_t GetState() { return state_; }
  ALWAYS_INLINE static void SetState(uintptr_t value) { state_ = value; }

 private:
  static thread_local uintptr_t state_
      __attribute__((tls_model("initial-exec")));
#else
  ALWAYS_INLINE static uintptr_t GetState() {
    DCHECK(tls_key_);
    return reinterpret_cast<uintptr_t>(
        partition_alloc::internal::PartitionTlsGet(tls_key_));
  }

  ALWAYS_INLINE static void SetState(uintptr_t value) {
    DCHECK(tls_key_);
    partition_alloc::internal::PartitionTlsSet(tls_key_,
                                               reinterpret_cast<void*>(value));
  }

 private:
  static partition_alloc::internal::PartitionTlsKey tls_key_;
#endif
};

#if !defined(THREAD_LOCAL_STATE_USES_PARTITION_ALLOC_TLS)
template <typename T>
thread_local uintptr_t ThreadLocalState<T>::state_ = 0;
#else
template <typename T>
partition_alloc::internal::PartitionTlsKey ThreadLocalState<T>::tls_key_ = 0;
#endif

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_THREAD_LOCAL_STATE_H_
