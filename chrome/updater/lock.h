// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LOCK_H_
#define CHROME_UPDATER_LOCK_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

enum class UpdaterScope;
class ScopedLockImpl;

// ScopedLock represents a held lock. Destroying the ScopedLock releases the
// lock. ScopedLock is reentrant on windows, but not on mac or linux.
class ScopedLock {
 public:
  explicit ScopedLock(std::unique_ptr<ScopedLockImpl> impl);
  ScopedLock(const ScopedLock&) = delete;
  ScopedLock& operator=(const ScopedLock&) = delete;
  ~ScopedLock();

  // Returns a ScopedLock, or nullptr if the lock could not be acquired within
  // the timeout. While the ScopedLock exists, no other process on the machine
  // may acquire that lock.
  static std::unique_ptr<ScopedLock> Create(const std::string& name,
                                            UpdaterScope scope,
                                            base::TimeDelta timeout);

 private:
  std::unique_ptr<ScopedLockImpl> impl_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_LOCK_H_
