// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_system_lock/lock.h"

#include <windows.h>

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/atl.h"
#include "base/win/scoped_handle.h"

namespace named_system_lock {

class ScopedLockImpl {
 public:
  ScopedLockImpl() = default;
  ScopedLockImpl(const ScopedLockImpl&) = delete;
  ScopedLockImpl& operator=(const ScopedLockImpl&) = delete;
  ~ScopedLockImpl();

 private:
  friend ScopedLock;
  bool Initialize(const std::wstring& mutex_name,
                  CSecurityAttributes* sa,
                  base::TimeDelta timeout);

  base::win::ScopedHandle mutex_;
};

ScopedLock::ScopedLock(std::unique_ptr<ScopedLockImpl> impl)
    : impl_(std::move(impl)) {}

ScopedLock::~ScopedLock() = default;

// static
std::unique_ptr<ScopedLock> ScopedLock::Create(const std::wstring& mutex_name,
                                               CSecurityAttributes* sa,
                                               base::TimeDelta timeout) {
  auto lock = std::make_unique<ScopedLockImpl>();

  VLOG(2) << "Trying to acquire the lock: " << mutex_name;
  if (!lock->Initialize(mutex_name, sa, timeout)) {
    return nullptr;
  }
  VLOG(2) << "Lock acquired: " << mutex_name;

  return std::make_unique<ScopedLock>(std::move(lock));
}

bool ScopedLockImpl::Initialize(const std::wstring& mutex_name,
                                CSecurityAttributes* sa,
                                base::TimeDelta timeout) {
  mutex_.Set(::CreateMutex(sa, false, mutex_name.c_str()));
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

}  // namespace named_system_lock
