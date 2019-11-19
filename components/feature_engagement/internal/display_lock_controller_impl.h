// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_DISPLAY_LOCK_CONTROLLER_IMPL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_DISPLAY_LOCK_CONTROLLER_IMPL_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/feature_engagement/internal/display_lock_controller.h"
#include "components/feature_engagement/public/tracker.h"

namespace feature_engagement {
class DisplayLockHandle;

// The default implementation of the DisplayLockController.
class DisplayLockControllerImpl : public DisplayLockController {
 public:
  DisplayLockControllerImpl();
  ~DisplayLockControllerImpl() override;

  // DisplayLockController implementation.
  std::unique_ptr<DisplayLockHandle> AcquireDisplayLock() override;
  bool IsDisplayLocked() const override;

 private:
  // Callback invoked when a DisplayLockHandle is released.
  void ReleaseDisplayLock(uint32_t handle_id);

  // Contains all currently outstanding display locks.
  std::set<uint32_t> outstanding_display_locks_;

  // The ID to use for the next handle.
  uint32_t next_handle_id_ = 1;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<DisplayLockControllerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DisplayLockControllerImpl);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_DISPLAY_LOCK_CONTROLLER_IMPL_H_
