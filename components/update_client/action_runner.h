// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_
#define COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/update_client/component_unpacker.h"

namespace base {
class CommandLine;
class Process;
class SingleThreadTaskRunner;
}

namespace update_client {

class Component;

class ActionRunner {
 public:
  using Callback =
      base::OnceCallback<void(bool succeeded, int error_code, int extra_code1)>;

  explicit ActionRunner(const Component& component);
  ~ActionRunner();

  void Run(Callback run_complete);

 private:
  void RunOnTaskRunner(std::unique_ptr<Unzipper> unzipper,
                       scoped_refptr<Patcher> patcher);
  void UnpackComplete(const ComponentUnpacker::Result& result);

  void RunCommand(const base::CommandLine& cmdline);
  void RunRecoveryCRXElevated(const base::FilePath& crx_path);

  base::CommandLine MakeCommandLine(const base::FilePath& unpack_path) const;

  void WaitForCommand(base::Process process);

#if defined(OS_WIN)
  void RunRecoveryCRXElevatedInSTA(const base::FilePath& crx_path);
#endif

  bool is_per_user_install_ = false;
  const Component& component_;

  // Used to post callbacks to the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Contains the unpack path for the component associated with the run action.
  base::FilePath unpack_path_;

  Callback run_complete_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(ActionRunner);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_
