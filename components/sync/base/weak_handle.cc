// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/weak_handle.h"

#include <sstream>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"

namespace syncer::internal {

WeakHandleCoreBase::WeakHandleCoreBase()
    : owner_loop_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

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
