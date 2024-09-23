// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LOCK_H_
#define CHROME_UPDATER_LOCK_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "chrome/updater/updater_scope.h"
#include "components/named_system_lock/lock.h"

namespace updater {

using ScopedLock = ::named_system_lock::ScopedLock;

// Returns a ScopedLock, or nullptr if the lock could not be acquired within
// the timeout. While the ScopedLock exists, no other process on the machine
// may acquire that lock.
std::unique_ptr<ScopedLock> CreateScopedLock(const std::string& name,
                                             UpdaterScope scope,
                                             base::TimeDelta timeout);
}  // namespace updater

#endif  // CHROME_UPDATER_LOCK_H_
