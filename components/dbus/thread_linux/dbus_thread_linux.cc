// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/thread_linux/dbus_thread_linux.h"

#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/single_thread_task_runner.h"

namespace dbus_thread_linux {

namespace {

// Use TaskPriority::USER_BLOCKING, because there is a client
// (NotificationPlatformBridgeLinuxImpl) which needs to run user-blocking tasks
// on this thread. Use SingleThreadTaskRunnerThreadMode::SHARED, because DBus
// does not require an exclusive use of the thread, only the existence of a
// single thread for all tasks.
base::LazyThreadPoolSingleThreadTaskRunner g_dbus_thread_task_runner =
    LAZY_THREAD_POOL_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_BLOCKING),
        base::SingleThreadTaskRunnerThreadMode::SHARED);

}  // namespace

scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
  return g_dbus_thread_task_runner.Get();
}

}  // namespace dbus_thread_linux
