// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_SHARED_STATE_H_
#define COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_SHARED_STATE_H_

#include <stdint.h>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"

namespace gwp_asan::internal::lud {

template <typename T>
class SharedState;

template <typename T>
class SharedStateHolder {
 private:
  static bool initialized_;
  static uint8_t buffer_[];

  friend class SharedState<T>;
};

template <typename T>
bool SharedStateHolder<T>::initialized_ = false;

template <typename T>
alignas(T) uint8_t SharedStateHolder<T>::buffer_[sizeof(T)];

// Special purpose shared state type. Uses a static buffer to reduce the number
// of pointer dereferences, requires explicit initialization, and doesn't
// provide thread safety.
template <typename T>
class SharedState {
 public:
  template <typename... Args>
  static void Init(Args&&... args) {
    DCHECK(!Holder::initialized_);
    Holder::initialized_ = true;
    new (Holder::buffer_) T(std::forward<Args>(args)...);
  }

  ALWAYS_INLINE static T* Get() {
    DCHECK(Holder::initialized_);
    return reinterpret_cast<T*>(Holder::buffer_);
  }

  static void ResetForTesting() {
    if (Holder::initialized_) {
      Get()->~T();
      Holder::initialized_ = false;
    }
  }

 private:
  using Holder = SharedStateHolder<T>;
};

}  // namespace gwp_asan::internal::lud

#endif  // COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_SHARED_STATE_H_
