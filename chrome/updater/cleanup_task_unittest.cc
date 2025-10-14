// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/cleanup_task.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/util/win_util.h"
#endif

namespace updater {

class CleanupTaskTest : public testing::Test {
 protected:
  void TearDown() override {
    base::DeletePathRecursively(
        *GetInstallDirectory(GetUpdaterScopeForTesting()));
  }
};

TEST_F(CleanupTaskTest, RunCleanupObsoleteFiles) {
  base::test::TaskEnvironment task_environment;
#if BUILDFLAG(IS_POSIX)
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  // Set up a mock `GoogleUpdate.exe`, and the following mock directories:
  // `Download`, `Install`, and a versioned `1.2.3.4` directory.
  const std::optional<base::FilePath> google_update_exe =
      GetGoogleUpdateExePath(GetUpdaterScopeForTesting());
  ASSERT_TRUE(google_update_exe.has_value());
  test::SetupMockUpdater(google_update_exe.value());
#endif  // BUILDFLAG(IS_WIN)

  base::FilePath chrome_url_fetcher_dir;
  base::FilePath chrome_unpacker_dir;
  base::FilePath chrome_bits_dir;
  base::FilePath random_temp_dir;

  // Set up mock update_client temp directories.
  ASSERT_TRUE(base::CreateNewTempDirectory(
      FILE_PATH_LITERAL("chrome_url_fetcher_BazBar"), &chrome_url_fetcher_dir));
  ASSERT_TRUE(base::CreateNewTempDirectory(
      FILE_PATH_LITERAL("chrome_Unpacker_BeginUnzippingBazBar"),
      &chrome_unpacker_dir));
  ASSERT_TRUE(base::CreateNewTempDirectory(
      FILE_PATH_LITERAL("chrome_BITS_BazBar"), &chrome_bits_dir));
  ASSERT_TRUE(base::CreateNewTempDirectory(FILE_PATH_LITERAL("random_temp"),
                                           &random_temp_dir));

  std::optional<base::FilePath> folder_path = GetVersionedInstallDirectory(
      GetUpdaterScopeForTesting(), base::Version("100"));
  ASSERT_TRUE(folder_path);
  ASSERT_TRUE(base::CreateDirectory(*folder_path));
  std::optional<base::FilePath> folder_path_current =
      GetVersionedInstallDirectory(GetUpdaterScopeForTesting(),
                                   base::Version(kUpdaterVersion));
  ASSERT_TRUE(folder_path_current);
  ASSERT_TRUE(base::CreateDirectory(*folder_path_current));

  const UpdaterScope scope = GetUpdaterScopeForTesting();
  auto cleanup_task = base::MakeRefCounted<CleanupTask>(
      scope, base::MakeRefCounted<Configurator>(
                 CreateGlobalPrefs(scope), CreateExternalConstants(), scope));
  base::RunLoop run_loop;
  cleanup_task->Run(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_FALSE(base::PathExists(*folder_path));
  ASSERT_TRUE(base::PathExists(*folder_path_current));

  // Expect all the mock update_client temp directories to be cleaned up.
  EXPECT_FALSE(base::PathExists(chrome_url_fetcher_dir));
  EXPECT_FALSE(base::PathExists(chrome_unpacker_dir));
  EXPECT_FALSE(base::PathExists(chrome_bits_dir));
  EXPECT_TRUE(base::PathExists(random_temp_dir));
  EXPECT_TRUE(base::DeletePathRecursively(random_temp_dir));

#if BUILDFLAG(IS_WIN)
  // Expect only a single file `GoogleUpdate.exe` and nothing else under
  // `\Google\Update`.
  test::ExpectOnlyMockUpdater(google_update_exe.value());
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace updater
