// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/unittest_util.h"

#include "base/files/file_path.h"
#include "base/process/kill.h"
#include "base/process/process_iterator.h"
#include "base/time/time.h"

namespace updater::test {

const char kChromeAppId[] = "{8A69D345-D564-463C-AFF1-A69D9E530F96}";

bool IsProcessRunning(const base::FilePath::StringType& executable_name) {
  return base::GetProcessCount(executable_name, nullptr) != 0;
}

bool WaitForProcessesToExit(const base::FilePath::StringType& executable_name,
                            base::TimeDelta wait) {
  return base::WaitForProcessesToExit(executable_name, wait, nullptr);
}

bool KillProcesses(const base::FilePath::StringType& executable_name,
                   int exit_code) {
  return base::KillProcesses(executable_name, exit_code, nullptr);
}

}  // namespace updater::test
