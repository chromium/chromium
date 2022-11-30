// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_debug_exception_handler_win.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"
#include "native_client/src/public/win/debug_exception_handler.h"

namespace {

class DebugExceptionHandler : public base::PlatformThread::Delegate {
 public:
  DebugExceptionHandler(base::Process nacl_process,
                        const std::string& startup_info,
                        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        base::RepeatingCallback<void(bool)> on_connected)
      : nacl_process_(std::move(nacl_process)),
        startup_info_(startup_info),
        task_runner_(task_runner),
        on_connected_(std::move(on_connected)) {}

  DebugExceptionHandler(const DebugExceptionHandler&) = delete;
  DebugExceptionHandler& operator=(const DebugExceptionHandler&) = delete;

  void ThreadMain() override {
    // In the Windows API, the set of processes being debugged is
    // thread-local, so we have to attach to the process (using
    // DebugActiveProcess()) on the same thread on which
    // NaClDebugExceptionHandlerRun() receives debug events for the
    // process.
    bool attached = false;
    int pid = nacl_process_.Pid();
    if (nacl_process_.IsValid()) {
      DCHECK(pid);
      if (!DebugActiveProcess(pid)) {
        LOG(ERROR) << "Failed to connect to the process";
      } else {
        attached = true;
      }
    } else {
      LOG(ERROR) << "Invalid process handle";
    }
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(on_connected_), attached));

    if (attached) {
      NaClDebugExceptionHandlerRun(
          nacl_process_.Handle(),
          reinterpret_cast<const void*>(startup_info_.data()),
          startup_info_.size());
    }
    delete this;
  }

 private:
  base::Process nacl_process_;
  std::string startup_info_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::RepeatingCallback<void(bool)> on_connected_;
};

}  // namespace

void NaClStartDebugExceptionHandlerThread(
    base::Process nacl_process,
    const std::string& startup_info,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::RepeatingCallback<void(bool)> on_connected) {
  // The new PlatformThread will take ownership of the
  // DebugExceptionHandler object, which will delete itself on exit.
  DebugExceptionHandler* handler = new DebugExceptionHandler(
      std::move(nacl_process), startup_info, task_runner, on_connected);
  if (!base::PlatformThread::CreateNonJoinable(0, handler)) {
    on_connected.Run(false);
    delete handler;
  }
}
