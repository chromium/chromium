// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/recursive_mutex.h"

namespace nearby::chrome {

RecursiveMutex::RecursiveMutex() = default;

RecursiveMutex::~RecursiveMutex() {
#if DCHECK_IS_ON()
  base::AutoLock al(bookkeeping_lock_);
  DCHECK_EQ(0u, num_acquisitions_);
  DCHECK_EQ(base::kInvalidThreadId, owning_thread_id_);
#endif  // DCHECK_IS_ON()
}

void RecursiveMutex::Lock() EXCLUSIVE_LOCK_FUNCTION() {
  {
    base::AutoLock al(bookkeeping_lock_);
    if (num_acquisitions_ > 0u &&
        owning_thread_id_ == base::PlatformThread::CurrentId()) {
      real_lock_.AssertAcquired();
      ++num_acquisitions_;
      return;
    }
  }

  // At this point, either no thread currently holds |real_lock_|, in which case
  // the current thread should be able to immediately acquire it, or a different
  // thread holds it, in which case Acquire() will block. It's necessary that
  // Acquire() happens outside the critical sections of |bookkeeping_lock_|,
  // otherwise any future calls to Unlock() will block on acquiring
  // |bookkeeping_lock_|, which would prevent Release() from ever running on
  // |real_lock_|, resulting in deadlock.
  real_lock_.Acquire();

  {
    base::AutoLock al(bookkeeping_lock_);
    DCHECK_EQ(0u, num_acquisitions_);
    owning_thread_id_ = base::PlatformThread::CurrentId();
    num_acquisitions_ = 1;
  }
}

void RecursiveMutex::Unlock() UNLOCK_FUNCTION() {
  base::AutoLock al(bookkeeping_lock_);
  DCHECK_GT(num_acquisitions_, 0u);
  DCHECK_EQ(base::PlatformThread::CurrentId(), owning_thread_id_);
  real_lock_.AssertAcquired();

  --num_acquisitions_;
  if (num_acquisitions_ == 0u) {
    owning_thread_id_ = base::kInvalidThreadId;
    real_lock_.Release();
  }
}

}  // namespace nearby::chrome
