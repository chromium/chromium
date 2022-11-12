// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs_impl.h"

#include <windows.h>

#include <memory>

#include "base/logging.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

class ScopedPrefsLockImpl {
 public:
  ScopedPrefsLockImpl() = default;
  ScopedPrefsLockImpl(const ScopedPrefsLockImpl&) = delete;
  ScopedPrefsLockImpl& operator=(const ScopedPrefsLockImpl&) = delete;
  ~ScopedPrefsLockImpl();

  bool Initialize(UpdaterScope scope, base::TimeDelta timeout);

 private:
  base::win::ScopedHandle mutex_;
};

ScopedPrefsLock::ScopedPrefsLock(std::unique_ptr<ScopedPrefsLockImpl> impl)
    : impl_(std::move(impl)) {}

ScopedPrefsLock::~ScopedPrefsLock() = default;

std::unique_ptr<ScopedPrefsLock> AcquireGlobalPrefsLock(
    UpdaterScope scope,
    base::TimeDelta timeout) {
  auto lock = std::make_unique<ScopedPrefsLockImpl>();

  VLOG(2) << "Trying to acquire the lock.";
  if (!lock->Initialize(scope, timeout))
    return nullptr;
  VLOG(2) << "Lock acquired.";

  return std::make_unique<ScopedPrefsLock>(std::move(lock));
}

bool ScopedPrefsLockImpl::Initialize(UpdaterScope scope,
                                     base::TimeDelta timeout) {
  NamedObjectAttributes lock_attr =
      GetNamedObjectAttributes(kPrefsAccessMutex, scope);
  mutex_.Set(::CreateMutex(&lock_attr.sa, false, lock_attr.name.c_str()));
  if (!mutex_.IsValid())
    return false;

  DWORD ret = ::WaitForSingleObject(mutex_.Get(), timeout.InMilliseconds());
  return ret == WAIT_OBJECT_0 || ret == WAIT_ABANDONED;
}

ScopedPrefsLockImpl::~ScopedPrefsLockImpl() {
  if (mutex_.IsValid()) {
    ::ReleaseMutex(mutex_.Get());
    VLOG(2) << "Lock released.";
  }
}

}  // namespace updater
