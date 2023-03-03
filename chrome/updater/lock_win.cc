// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/lock.h"

#include <windows.h>

#include <memory>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

class ScopedLockImpl {
 public:
  ScopedLockImpl() = default;
  ScopedLockImpl(const ScopedLockImpl&) = delete;
  ScopedLockImpl& operator=(const ScopedLockImpl&) = delete;
  ~ScopedLockImpl();

  bool Initialize(const std::string& id,
                  UpdaterScope scope,
                  base::TimeDelta timeout);

 private:
  base::win::ScopedHandle mutex_;
};

ScopedLock::ScopedLock(std::unique_ptr<ScopedLockImpl> impl)
    : impl_(std::move(impl)) {}

ScopedLock::~ScopedLock() = default;

// static
std::unique_ptr<ScopedLock> ScopedLock::Create(const std::string& name,
                                               UpdaterScope scope,
                                               base::TimeDelta timeout) {
  auto lock = std::make_unique<ScopedLockImpl>();

  VLOG(2) << "Trying to acquire the lock: " << name << ": " << scope;
  if (!lock->Initialize(name, scope, timeout)) {
    return nullptr;
  }
  VLOG(2) << "Lock acquired: " << name << ": " << scope;

  return std::make_unique<ScopedLock>(std::move(lock));
}

bool ScopedLockImpl::Initialize(const std::string& name,
                                UpdaterScope scope,
                                base::TimeDelta timeout) {
  NamedObjectAttributes lock_attr =
      GetNamedObjectAttributes(base::ASCIIToWide(name).c_str(), scope);
  mutex_.Set(::CreateMutex(&lock_attr.sa, false, lock_attr.name.c_str()));
  if (!mutex_.IsValid()) {
    return false;
  }

  DWORD ret = ::WaitForSingleObject(mutex_.Get(), timeout.InMilliseconds());
  return ret == WAIT_OBJECT_0 || ret == WAIT_ABANDONED;
}

ScopedLockImpl::~ScopedLockImpl() {
  if (mutex_.IsValid()) {
    ::ReleaseMutex(mutex_.Get());
    VLOG(2) << "Lock released.";
  }
}

}  // namespace updater
