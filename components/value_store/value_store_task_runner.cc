// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/value_store_task_runner.h"

#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"

namespace value_store {

namespace {

// Note: All tasks posted to a single task runner have the same priority. This
// is unfortunate, since some file-related tasks are high priority, and others
// are low priority (like garbage collection). Split the difference and use
// USER_VISIBLE, which is the default priority and what a task posted to a
// named thread (like the FILE thread) would receive.
base::LazyThreadPoolSequencedTaskRunner g_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
                         base::TaskPriority::USER_VISIBLE));

}  // namespace

scoped_refptr<base::SequencedTaskRunner> GetValueStoreTaskRunner() {
  return g_task_runner.Get();
}

}  // namespace value_store
