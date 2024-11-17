// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_PROCESS_WATCHER_H_
#define CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_PROCESS_WATCHER_H_

#include "base/functional/callback_forward.h"
#include "base/process/process.h"
#include "base/synchronization/waitable_event.h"

namespace elevated_tracing_service {

// Runs a closure when a watched process terminates.
class ProcessWatcher {
 public:
  // Starts watching `process` for termination. `process` must have SYNCHRONIZE
  // rights. `on_terminated` will be run if/when the process terminates.
  ProcessWatcher(base::Process process, base::OnceClosure on_terminated);
  ProcessWatcher(const ProcessWatcher&) = delete;
  ProcessWatcher& operator=(const ProcessWatcher&) = delete;
  ~ProcessWatcher();

 private:
  class ThreadDelegate;

  // An event that is signaled at destruction to cancel the watch.
  base::WaitableEvent shutdown_event_;
};

}  // namespace elevated_tracing_service

#endif  // CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_PROCESS_WATCHER_H_
