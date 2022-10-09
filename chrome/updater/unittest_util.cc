// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/unittest_util.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/process_iterator.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

absl::optional<base::FilePath> GetOverrideFilePath(UpdaterScope scope) {
  const absl::optional<base::FilePath> data_dir = GetBaseDataDirectory(scope);
  return data_dir
             ? absl::make_optional(data_dir->AppendASCII(kDevOverrideFileName))
             : absl::nullopt;
}

bool DeleteFileAndEmptyParentDirectories(
    const absl::optional<base::FilePath>& file_path) {
  struct Local {
    // Deletes recursively `dir` and its parents up, if dir is empty
    // and until one non-empty parent directory is found.
    static bool DeleteDirsIfEmpty(const base::FilePath& dir) {
      if (!base::DirectoryExists(dir) || !base::IsDirectoryEmpty(dir))
        return true;
      if (!base::DeleteFile(dir))
        return false;
      return DeleteDirsIfEmpty(dir.DirName());
    }
  };

  if (!file_path || !base::DeleteFile(*file_path))
    return false;
  return Local::DeleteDirsIfEmpty(file_path->DirName());
}

base::FilePath GetUpdaterTestPath() {
  base::FilePath out_dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &out_dir));
#if BUILDFLAG(IS_WIN)
  return out_dir.Append(FILE_PATH_LITERAL("updater_test.exe"));
#else
  return out_dir.Append(FILE_PATH_LITERAL("updater_test"));
#endif
}

}  // namespace updater::test
