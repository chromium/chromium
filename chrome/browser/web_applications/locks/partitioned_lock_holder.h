// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_HOLDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_HOLDER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/partitioned_lock.h"

namespace web_app {

// Used to receive and hold locks from a PartitionedLockManager. This class
// enables the PartitionedLock objects to always live in the destination of the
// caller's choosing (as opposed to having the locks be an argument in the
// callback, where they could be owned by the task scheduler).
//
// This class must be used and destructed on the same sequence as the
// PartitionedLockManager.
class PartitionedLockHolder {
 public:
  PartitionedLockHolder();
  PartitionedLockHolder(const PartitionedLockHolder&) = delete;
  PartitionedLockHolder& operator=(const PartitionedLockHolder&) = delete;
  ~PartitionedLockHolder();

  base::WeakPtr<PartitionedLockHolder> AsWeakPtr();

  // Returns if all locks have been granted for this holder.
  bool is_locked() const;

  // Releases all locks held by this holder, also invalidating any pending lock
  // requests. All WeakPtrs are invalidated.
  void Release();

 private:
  friend class PartitionedLockManager;
  std::vector<PartitionedLock> locks_;
  bool is_locked_ = false;
  base::WeakPtrFactory<PartitionedLockHolder> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_HOLDER_H_
