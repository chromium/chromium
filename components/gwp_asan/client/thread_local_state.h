// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_THREAD_LOCAL_STATE_H_
#define COMPONENTS_GWP_ASAN_CLIENT_THREAD_LOCAL_STATE_H_

#include "base/check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
#define USE_PTHREAD_TLS
#include <pthread.h>
#endif

namespace gwp_asan::internal {

// Class that provides the optimal thread-local storage implementation depending
// on the platform. On macOS and Android (before Q), the first use of a
// `thread_local` variable on a new thread will cause an allocation, leading to
// infinite recursion. Also, `thread_local` goes through `emutls` on Android,
// which is slower than `pthread_getspecific`.
template <typename T>
class ThreadLocalState {
 protected:
  static void InitIfNeeded() {
#if defined(USE_PTHREAD_TLS)
    if (!tls_key_) {
      pthread_key_create(&tls_key_, nullptr);
    }
#endif
  }

#if !defined(USE_PTHREAD_TLS)
  ALWAYS_INLINE static uintptr_t GetState() { return state_; }
  ALWAYS_INLINE static void SetState(uintptr_t value) { state_ = value; }

 private:
  static thread_local uintptr_t state_;
#else
  ALWAYS_INLINE static uintptr_t GetState() {
    DCHECK(tls_key_);
    return reinterpret_cast<uintptr_t>(pthread_getspecific(tls_key_));
  }

  ALWAYS_INLINE static void SetState(uintptr_t value) {
    DCHECK(tls_key_);
    pthread_setspecific(tls_key_, reinterpret_cast<void*>(value));
  }

 private:
  static pthread_key_t tls_key_;
#endif
};

#if !defined(USE_PTHREAD_TLS)
template <typename T>
thread_local uintptr_t ThreadLocalState<T>::state_ = 0;
#else
template <typename T>
pthread_key_t ThreadLocalState<T>::tls_key_ = 0;
#endif

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_THREAD_LOCAL_STATE_H_
