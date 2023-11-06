// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_SHARED_STATE_H_
#define COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_SHARED_STATE_H_

#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"

namespace gwp_asan::internal {

// Special purpose shared state type. Uses a static buffer to reduce the number
// of pointer dereferences, requires explicit initialization, and doesn't
// provide thread safety.
template <typename T>
class SharedState {
 public:
  template <typename... Args>
  static void Init(Args&&... args) {
    instance_initialized_ = true;
    new (Get()) T(std::forward<Args>(args)...);
  }

  ALWAYS_INLINE static T* Get() {
    DCHECK(instance_initialized_);
    return reinterpret_cast<T*>(instance_buffer_);
  }

  static void ResetForTesting() {
    if (instance_initialized_) {
      Get()->~T();
      instance_initialized_ = false;
    }
  }

 private:
  static bool instance_initialized_;
  static uint8_t instance_buffer_[];
};

template <typename T>
bool SharedState<T>::instance_initialized_ = false;

template <typename T>
alignas(T) uint8_t SharedState<T>::instance_buffer_[sizeof(T)];

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_SHARED_STATE_H_
