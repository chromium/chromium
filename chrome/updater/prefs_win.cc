// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs_impl.h"

#include <windows.h>

#include <memory>

#include "base/logging.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/util.h"

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
    base::TimeDelta timeout) {
  auto lock = std::make_unique<ScopedPrefsLockImpl>();

  // TODO(crbug.com/1096654): need to pass 'scope' instead of
  // 'UpdaterScope::kUser' here.
  DVLOG(2) << "Trying to acquire the lock.";
  if (!lock->Initialize(UpdaterScope::kUser, timeout))
    return nullptr;
  DVLOG(2) << "Lock acquired.";

  return std::make_unique<ScopedPrefsLock>(std::move(lock));
}

bool ScopedPrefsLockImpl::Initialize(UpdaterScope scope,
                                     base::TimeDelta timeout) {
  NamedObjectAttributes lock_attr;
  GetNamedObjectAttributes(kPrefsAccessMutex, scope, &lock_attr);
  mutex_.Set(::CreateMutex(&lock_attr.sa, false, lock_attr.name.c_str()));
  if (!mutex_.IsValid())
    return false;

  DWORD ret = ::WaitForSingleObject(mutex_.Get(), timeout.InMilliseconds());
  return ret == WAIT_OBJECT_0 || ret == WAIT_ABANDONED;
}

ScopedPrefsLockImpl::~ScopedPrefsLockImpl() {
  if (mutex_.IsValid()) {
    ::ReleaseMutex(mutex_.Get());
    DVLOG(2) << "Lock released.";
  }
}

}  // namespace updater
