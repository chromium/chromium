// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_
#define CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_

#include "chrome/common/service_process_util.h"

#include <signal.h>

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MAC)
#include "chrome/common/multi_process_lock.h"
std::unique_ptr<MultiProcessLock> TakeServiceRunningLock();
#endif

#if defined(OS_MAC)
#include "base/files/file_path_watcher.h"
#include "base/mac/scoped_cftyperef.h"
#include "chrome/common/mac/service_management.h"

namespace base {
class CommandLine;
}

mac::services::JobOptions GetServiceProcessJobOptions(
    base::CommandLine* cmd_line,
    bool for_auto_launch);
#endif  // OS_MAC

namespace base {
class WaitableEvent;
}

// Watches for |kTerminateMessage| to be written to the file descriptor it is
// watching. When it reads |kTerminateMessage|, it performs |terminate_task_|.
// Used here to monitor the socket listening to g_signal_socket.
class ServiceProcessTerminateMonitor
    : public base::MessagePumpForIO::FdWatcher {
 public:

  enum {
    kTerminateMessage = 0xdecea5e
  };

  explicit ServiceProcessTerminateMonitor(base::OnceClosure terminate_task);
  ~ServiceProcessTerminateMonitor() override;

  // MessagePumpForIO::FdWatcher overrides
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  base::OnceClosure terminate_task_;
};

struct ServiceProcessState::StateData {
  StateData();
  ~StateData();

  // WatchFileDescriptor needs to be set up by the thread that is going
  // to be monitoring it.
  void SignalReady(base::WaitableEvent* signal, bool* success);

#if defined(OS_MAC)
  bool WatchExecutable();

  mac::services::JobCheckinInfo job_info;
  base::FilePathWatcher executable_watcher;
#else
  std::unique_ptr<MultiProcessLock> initializing_lock;
  std::unique_ptr<MultiProcessLock> running_lock;
#endif
  std::unique_ptr<ServiceProcessTerminateMonitor> terminate_monitor;
  base::MessagePumpForIO::FdWatchController watcher;
  int sockets[2];
  struct sigaction old_action;
  bool set_action;

  // The SingleThreadTaskRunner on which SignalReady and the destructor are
  // invoked.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
};

#endif  // CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_
