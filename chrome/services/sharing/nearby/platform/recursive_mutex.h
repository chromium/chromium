// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_RECURSIVE_MUTEX_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_RECURSIVE_MUTEX_H_

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "third_party/nearby/src/internal/platform/implementation/mutex.h"

namespace nearby::chrome {

// Concrete Mutex implementation with recursive lock.
// As base::Lock does not support recursive locking, this class uses separate
// variable to track number of times a thread has acquried this mutex, and only
// acquire base::Lock on first acquisation per thread, and only release
// on the last release base::Lock per thread.
class RecursiveMutex : public api::Mutex {
 public:
  RecursiveMutex();
  ~RecursiveMutex() override;

  RecursiveMutex(const RecursiveMutex&) = delete;
  RecursiveMutex& operator=(const RecursiveMutex&) = delete;

  // Mutex:
  void Lock() override;
  void Unlock() override;

 private:
  friend class RecursiveMutexTest;

  // The underlying lock that can only be acquried once per thread.
  base::Lock real_lock_;
  // The lock that guards book keeping variables.
  base::Lock bookkeeping_lock_;
  base::PlatformThreadId owning_thread_id_ GUARDED_BY(bookkeeping_lock_) =
      base::kInvalidThreadId;
  size_t num_acquisitions_ GUARDED_BY(bookkeeping_lock_) = 0;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_RECURSIVE_MUTEX_H_
