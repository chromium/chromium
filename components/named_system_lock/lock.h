// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_SYSTEM_LOCK_LOCK_H_
#define COMPONENTS_NAMED_SYSTEM_LOCK_LOCK_H_

#include <memory>
#include <string>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/atl.h"
#endif

namespace base {
class TimeDelta;
}  // namespace base

namespace named_system_lock {

class ScopedLockImpl;

// ScopedLock represents a held lock. Destroying the ScopedLock releases the
// lock. ScopedLock is reentrant on Windows, but not on Mac or Linux.
class ScopedLock {
 public:
  explicit ScopedLock(std::unique_ptr<ScopedLockImpl> impl);
  ScopedLock(const ScopedLock&) = delete;
  ScopedLock& operator=(const ScopedLock&) = delete;
  ~ScopedLock();

  // Returns a ScopedLock, or nullptr if the lock could not be acquired within
  // the timeout. While the ScopedLock exists, no other process on the machine
  // may acquire that lock. The lock name has different meanings per platform:
  // Linux: A shared memory object name starting with `/`. E.g. `/MyApp.lock`.
  // Mac: A bootstrap service name (see `man bootstrap_check_in`).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  static std::unique_ptr<ScopedLock> Create(const std::string& name,
                                            base::TimeDelta timeout);
#elif BUILDFLAG(IS_WIN)
  static std::unique_ptr<ScopedLock> Create(const std::wstring& mutex_name,
                                            CSecurityAttributes* sa,
                                            base::TimeDelta timeout);
#endif

 private:
  std::unique_ptr<ScopedLockImpl> impl_;
};

}  // namespace named_system_lock

#endif  // COMPONENTS_NAMED_SYSTEM_LOCK_LOCK_H_
