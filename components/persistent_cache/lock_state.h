// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_LOCK_STATE_H_
#define COMPONENTS_PERSISTENT_CACHE_LOCK_STATE_H_

namespace persistent_cache {

// Values of this enum represent the state of a PersistentCache lock at a set
// moment in time. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class LockState {
  // The lock is not held by readers or writers.
  kNotHeld = 0,

  // One or more readers has acquired the lock. No writers hold it.
  kReading = 1,

  // A writer either holds the lock or is in the process of acquiring it. In the
  // latter case, there may remain one or more readers.
  kWriting = 2,

  kMaxValue = kWriting
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_LOCK_STATE_H_
