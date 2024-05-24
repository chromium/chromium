// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_LOCK_H_
#define CHROME_ENTERPRISE_COMPANION_LOCK_H_

#include <memory>

#include "components/named_system_lock/lock.h"

namespace enterprise_companion {

using ScopedLock = ::named_system_lock::ScopedLock;

// Returns a ScopedLock, or nullptr if the lock could not be acquired. While the
// ScopedLock exists, no other process on the machine may acquire that lock.
std::unique_ptr<ScopedLock> CreateScopedLock();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_LOCK_H_
