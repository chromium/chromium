// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NOOP_DISPLAY_LOCK_CONTROLLER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NOOP_DISPLAY_LOCK_CONTROLLER_H_

#include <memory>

#include "components/feature_engagement/internal/display_lock_controller.h"

namespace feature_engagement {
class DisplayLockHandle;

// NoopDisplayLockController never gives out display locks, and never assumes
// that the display is locked.
class NoopDisplayLockController : public DisplayLockController {
 public:
  NoopDisplayLockController();

  NoopDisplayLockController(const NoopDisplayLockController&) = delete;
  NoopDisplayLockController& operator=(const NoopDisplayLockController&) =
      delete;

  ~NoopDisplayLockController() override;

  // DisplayLockController implementation.
  std::unique_ptr<DisplayLockHandle> AcquireDisplayLock() override;
  bool IsDisplayLocked() const override;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NOOP_DISPLAY_LOCK_CONTROLLER_H_
