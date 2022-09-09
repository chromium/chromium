// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/unittest_util.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/kill.h"
#include "base/process/process_iterator.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "testing/gtest/include/gtest/gtest.h"

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

scoped_refptr<PolicyService> CreateTestPolicyService() {
  PolicyService::PolicyManagerVector managers;
  managers.push_back(GetDefaultValuesPolicyManager());
  return base::MakeRefCounted<PolicyService>(std::move(managers));
}

std::string GetTestName() {
  const ::testing::TestInfo* test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return test_info ? base::StrCat(
                         {test_info->test_suite_name(), ".", test_info->name()})
                   : "?.?";
}

}  // namespace updater::test
