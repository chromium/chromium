// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/elevated_tracing_service/process_watcher.h"

#include <windows.h>

#include <array>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_handle.h"

namespace elevated_tracing_service {

namespace {

// Waits for either the watched process to terminate or for the shutdown event
// to be signaled. In the former case, the `on_terminated` closure is
// run before exiting.
void WatchInThreadPool(base::Process process,
                       base::OnceClosure on_terminated,
                       HANDLE startup_event,
                       base::win::ScopedHandle shutdown_event) {
  // Signal that the task is ready to watch.
  ::SetEvent(std::exchange(startup_event, nullptr));

  DWORD result;
  {
    base::ScopedBlockingCall will_block(FROM_HERE,
                                        base::BlockingType::WILL_BLOCK);
    HANDLE handles[] = {process.Handle(), shutdown_event.get()};
    result = ::WaitForMultipleObjects(std::size(handles), &handles[0],
                                      /*bWaitAll=*/FALSE,
                                      /*dwMilliseconds=*/INFINITE);
  }
  CHECK_NE(result, WAIT_FAILED);
  if (result == WAIT_OBJECT_0) {
    std::move(on_terminated).Run();
  }  // else the shutdown event was signaled.
}

}  // namespace

ProcessWatcher::ProcessWatcher(base::Process process,
                               base::OnceClosure on_terminated) {
  // An event that is signaled by the watch task when it is ready to watch. No
  // need to duplicate this for the watch task, as this instance will outlive
  // the `SetEvent()` call in the task.
  base::WaitableEvent startup_event;

  // Prepare a dup of the shutdown event for the task to wait on.
  HANDLE shutdown_event = nullptr;
  CHECK(::DuplicateHandle(::GetCurrentProcess(), shutdown_event_.handle(),
                          ::GetCurrentProcess(), &shutdown_event,
                          /*dwDesiredAccess=*/0,
                          /*bInheritHandle=*/FALSE, DUPLICATE_SAME_ACCESS));

  base::ThreadPool::CreateTaskRunner(
      {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&WatchInThreadPool, std::move(process),
                         std::move(on_terminated), startup_event.handle(),
                         base::win::ScopedHandle(shutdown_event)));

  // Wait for the watch task to signal that it is ready.
  startup_event.Wait();
}

ProcessWatcher::~ProcessWatcher() {
  // Signal that the watch task should exit if it is still watching the process.
  shutdown_event_.Signal();
}

}  // namespace elevated_tracing_service
