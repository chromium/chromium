// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/noop_display_lock_controller.h"

#include "components/feature_engagement/public/tracker.h"

namespace feature_engagement {

NoopDisplayLockController::NoopDisplayLockController() = default;

NoopDisplayLockController::~NoopDisplayLockController() = default;

std::unique_ptr<DisplayLockHandle>
NoopDisplayLockController::AcquireDisplayLock() {
  return nullptr;
}

bool NoopDisplayLockController::IsDisplayLocked() const {
  return false;
}

}  // namespace feature_engagement
