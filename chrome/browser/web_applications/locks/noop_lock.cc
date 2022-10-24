// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/noop_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

NoopLockDescription::NoopLockDescription()
    : LockDescription({}, LockDescription::Type::kNoOp) {}
NoopLockDescription::~NoopLockDescription() = default;

}  // namespace web_app
