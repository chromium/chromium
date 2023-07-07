// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/cleanup_task.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/util/unit_test_util.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/util/win_util.h"
#endif

namespace updater {

class CleanupTaskTest : public testing::Test {};

TEST_F(CleanupTaskTest, RunCleanupObsoleteFiles) {
  base::test::TaskEnvironment task_environment;
// TODO(crbug.com/1428653): Fix this test to work for system scope.
#if BUILDFLAG(IS_POSIX)
  if (GetTestScope() == UpdaterScope::kSystem) {
    GTEST_SKIP();
  }
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  // Set up a mock `GoogleUpdate.exe`, and the following mock directories:
  // `Download`, `Install`, and a versioned `1.2.3.4` directory.
  const absl::optional<base::FilePath> google_update_exe =
      GetGoogleUpdateExePath(GetTestScope());
  ASSERT_TRUE(google_update_exe.has_value());
  test::SetupMockUpdater(google_update_exe.value());
#endif  // BUILDFLAG(IS_WIN)

  absl::optional<base::FilePath> folder_path =
      GetVersionedInstallDirectory(GetTestScope(), base::Version("100"));
  ASSERT_TRUE(folder_path);
  ASSERT_TRUE(base::CreateDirectory(*folder_path));

  auto cleanup_task = base::MakeRefCounted<CleanupTask>(GetTestScope());
  base::RunLoop run_loop;
  cleanup_task->Run(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_FALSE(base::PathExists(*folder_path));

#if BUILDFLAG(IS_WIN)
  // Expect only a single file `GoogleUpdate.exe` and nothing else under
  // `\Google\Update`.
  test::ExpectOnlyMockUpdater(google_update_exe.value());
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace updater
