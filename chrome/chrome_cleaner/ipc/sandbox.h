// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_IPC_SANDBOX_H_
#define CHROME_CHROME_CLEANER_IPC_SANDBOX_H_

#include <stdlib.h>

#include <map>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/os/process.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace chrome_cleaner {

using SandboxConnectionErrorCallback =
    base::RepeatingCallback<void(SandboxType)>;

// The suffix to append to log files for sandboxed processes.
extern const wchar_t kSandboxLogFileSuffix[];

// Functions that will be called while setting up the sandbox. Users of the
// sandbox can implement these to add their own features, such as IPC pipes.
class SandboxSetupHooks {
 public:
  SandboxSetupHooks();
  virtual ~SandboxSetupHooks();

  // Called before spawning the sandbox target process. |policy| is the set of
  // policies that will be applied to the target process. |command_line| is the
  // command-line it will be launched with. The callee can alter either of
  // these objects.
  //
  // If the return value is anything except RESULT_CODE_SUCCESS, the target
  // process will not be spawned and the return value will also be returned by
  // |StartSandboxTarget|.
  virtual ResultCode UpdateSandboxPolicy(sandbox::TargetPolicy* policy,
                                         base::CommandLine* command_line);

  // Called just after the target process is spawned, while it is still
  // suspended. |target_process| wraps a handle to the sandboxed process, and
  // |target_thread| wraps its main thread. The callee must duplicate these
  // handles if it wishes to save them.
  //
  // If the return value is anything except RESULT_CODE_SUCCESS, the target
  // process will be terminated and the return value will also be returned by
  // |StartSandboxTarget|.
  virtual ResultCode TargetSpawned(
      const base::Process& target_process,
      const base::win::ScopedHandle& target_thread);

  // Called when the target process is no longer suspended. When this returns
  // the sandbox setup is complete.
  //
  // If the return value is anything except RESULT_CODE_SUCCESS, the target
  // process will be terminated and the return value will also be returned by
  // |StartSandboxTarget|.
  virtual ResultCode TargetResumed();

  // Called as the last step of the sandbox setup if the process failed,
  // including if it failed because a method of SandboxSetupHooks returned an
  // error code.
  virtual void SetupFailed();
};

// Functions that will be called in the sandbox target process. Users of the
// sandbox must implement at least TargetDroppedPrivileges to provide the code
// that will run in the sandbox after lowering privileges.
class SandboxTargetHooks {
 public:
  SandboxTargetHooks();
  virtual ~SandboxTargetHooks();

  // Called after the process starts, before it has dropped all privileges. The
  // callee can do any setup which requires higher privileges here. It is
  // unsafe to call untrusted code or process untrusted data from this method.
  //
  // If the return value is anything except RESULT_CODE_SUCCESS, the target
  // process will exit using the return value as the exit code.
  virtual ResultCode TargetStartedWithHighPrivileges();

  // Called after TargetProcess::LowerToken. The callee should do all
  // processing that must be done in the sandbox here.
  //
  // The target process will exit using the return value as the exit code.
  virtual ResultCode TargetDroppedPrivileges(
      const base::CommandLine& command_line) = 0;
};

// Returns the type of process this sandbox is. This should only be called by
// sandboxed processes.
SandboxType SandboxProcessType();

// Spawns a new sandbox target with the given |setup_hooks| passed by
// parameters. The parameter |type| indicates the type of sandbox that is being
// created and therefore the switch kSandboxedProcessIdSwitch should be expected
// on the target's command line with the value specified on this parameter.
ResultCode SpawnSandbox(SandboxSetupHooks* setup_hooks, SandboxType type);

// Starts a sandbox target process with the command line
// |sandbox_command_line|.  If |hooks| is non-null its methods will be called
// during setup.
ResultCode StartSandboxTarget(const base::CommandLine& sandbox_command_line,
                              SandboxSetupHooks* hooks,
                              SandboxType type);

// Returns whether a sandbox target process is currently running or not.
bool IsSandboxTargetRunning(SandboxType type);

// Calls the methods of |hooks| and returns the result code of the last one to
// execute.  This will be called in the sandbox target process. |command_line|
// is the command line the process was launched with. |sandbox_target_services|
// should be the object returned by SandboxFactory::GetTargetServices() except
// when it is overridden in unit tests.
ResultCode RunSandboxTarget(const base::CommandLine& command_line,
                            sandbox::TargetServices* sandbox_target_services,
                            SandboxTargetHooks* hooks);

// Retrieves system resource usage stats for all sandbox target processes, even
// if the target processes have already exited.
std::map<SandboxType, SystemResourceUsage> GetSandboxSystemResourceUsage();

ResultCode GetResultCodeForSandboxConnectionError(SandboxType sandbox_type);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_IPC_SANDBOX_H_
