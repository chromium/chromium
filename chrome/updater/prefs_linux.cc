// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs_impl.h"

#include <memory>

#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

// TODO(crbug.com/1276171) - implement.
class ScopedPrefsLockImpl {
 public:
  ScopedPrefsLockImpl() = default;
  ScopedPrefsLockImpl(const ScopedPrefsLockImpl&) = delete;
  ScopedPrefsLockImpl& operator=(const ScopedPrefsLockImpl&) = delete;
  ~ScopedPrefsLockImpl() = default;
};

ScopedPrefsLock::ScopedPrefsLock(
    std::unique_ptr<ScopedPrefsLockImpl> /*impl*/) {}
ScopedPrefsLock::~ScopedPrefsLock() = default;

// TODO(crbug.com/1276171) - implement.
std::unique_ptr<ScopedPrefsLock> AcquireGlobalPrefsLock(
    UpdaterScope scope,
    base::TimeDelta timeout) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace updater
