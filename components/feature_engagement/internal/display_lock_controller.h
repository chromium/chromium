// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_DISPLAY_LOCK_CONTROLLER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_DISPLAY_LOCK_CONTROLLER_H_

#include <memory>

namespace feature_engagement {
class DisplayLockHandle;

// The DisplayLockController provides functionality for letting API users hold
// a lock to ensure no feature enlightenment is happening while any lock is
// held.
class DisplayLockController {
 public:
  DisplayLockController(const DisplayLockController&) = delete;
  DisplayLockController& operator=(const DisplayLockController&) = delete;

  virtual ~DisplayLockController() = default;

  // Acquiring a display lock means that no in-product help can be displayed
  // while it is held. To release the lock, delete the handle.
  virtual std::unique_ptr<DisplayLockHandle> AcquireDisplayLock() = 0;

  // Whether there currently are any held display locks.
  virtual bool IsDisplayLocked() const = 0;

 protected:
  DisplayLockController() = default;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_DISPLAY_LOCK_CONTROLLER_H_
