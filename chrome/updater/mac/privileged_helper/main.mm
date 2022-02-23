// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/platform_thread.h"
#include "chrome/updater/mac/privileged_helper/server.h"

int main(int argc, const char* argv[]) {
  base::PlatformThread::SetName("UpdaterHelperMain");
  base::CommandLine::Init(argc, argv);
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  return updater::PrivilegedHelperServerInstance()->Run();
}
