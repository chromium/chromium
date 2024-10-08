// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/display_lock_controller_impl.h"

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/threading/thread_checker.h"
#include "components/feature_engagement/public/tracker.h"

namespace feature_engagement {

DisplayLockControllerImpl::DisplayLockControllerImpl() = default;

DisplayLockControllerImpl::~DisplayLockControllerImpl() = default;

void DisplayLockControllerImpl::ReleaseDisplayLock(uint32_t handle_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = outstanding_display_locks_.find(handle_id);
  CHECK(it != outstanding_display_locks_.end(), base::NotFatalUntil::M130);
  outstanding_display_locks_.erase(it);
}

std::unique_ptr<DisplayLockHandle>
DisplayLockControllerImpl::AcquireDisplayLock() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint32_t handle_id = next_handle_id_++;
  DCHECK_EQ(0u, outstanding_display_locks_.count(handle_id));

  outstanding_display_locks_.insert(handle_id);
  return std::make_unique<DisplayLockHandle>(
      base::BindOnce(&DisplayLockControllerImpl::ReleaseDisplayLock,
                     weak_ptr_factory_.GetWeakPtr(), handle_id));
}

bool DisplayLockControllerImpl::IsDisplayLocked() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return !outstanding_display_locks_.empty();
}

}  // namespace feature_engagement
