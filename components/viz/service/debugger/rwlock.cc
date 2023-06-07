// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/debugger/rwlock.h"

namespace rwlock {

RWLock::RWLock() = default;

void RWLock::ReadLock() {
  int32_t expected = state_.load(std::memory_order_acquire);
  int32_t desired = expected + 1;

  if (expected == -1) {
    expected = 0;
    desired = 1;
  }

  while (!std::atomic_compare_exchange_weak(&state_, &expected, desired)) {
    if (expected == -1) {
      // A thread is writing. Wait for write-release to read.
      expected = 0;
    }
    desired = expected + 1;
  }
}
void RWLock::ReadUnlock() {
  state_--;
}

void RWLock::WriteLock() {
  // There should be no readers
  int32_t expected = 0;
  int32_t desired = -1;
  while (!std::atomic_compare_exchange_weak(&state_, &expected, desired)) {
    if (expected == -1) {
      // Another thread has the write lock.
    }
    expected = 0;
  }
}
void RWLock::WriteUnLock() {
  state_++;
}

}  // namespace rwlock
