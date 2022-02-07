// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/weak_handle.h"

#include <sstream>

#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace syncer::internal {

WeakHandleCoreBase::WeakHandleCoreBase()
    : owner_loop_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

bool WeakHandleCoreBase::IsOnOwnerThread() const {
  return owner_loop_task_runner_->RunsTasksInCurrentSequence();
}

WeakHandleCoreBase::~WeakHandleCoreBase() = default;

void WeakHandleCoreBase::PostToOwnerThread(const base::Location& from_here,
                                           base::OnceClosure fn) const {
  if (!owner_loop_task_runner_->PostTask(from_here, std::move(fn))) {
    DVLOG(1) << "Could not post task from " << from_here.ToString();
  }
}

}  // namespace syncer::internal
