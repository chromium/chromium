// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/path_util.h"

#include <memory>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/test/scoped_path_override.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "base/test/task_environment.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_id.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager::util {
namespace {

constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("user@example.com",
                                            GaiaId::Literal("12345"));

class FileManagerUserPathUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    ash::test::UserSessionTestEnvironment::RegisterLocalStatePrefs(
        local_state_.registry());
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(&local_state_);
  }

  void TearDown() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
    user_session_test_environment_.reset();
  }

  // Adds a regular user and logs in. Returns the logged in user.
  const user_manager::User* LogIn() {
    const user_manager::User* user =
        user_session_test_environment_->AddRegularUser(kTestAccountId);
    if (!user) {
      return nullptr;
    }
    user_session_test_environment_->LogIn(kTestAccountId);
    return user;
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedPathOverride user_data_dir_override_{
      ash::DIR_USER_DATA, base::FilePath("/home/chronos"),
      /*is_absolute=*/true, /*create=*/false};
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
};

TEST_F(FileManagerUserPathUtilTest, GetFolders) {
  const user_manager::User* user = LogIn();
  ASSERT_TRUE(user);
  ASSERT_FALSE(user->username_hash().empty());
  const base::FilePath user_dir =
      base::FilePath("/home/chronos").Append("u-" + user->username_hash());

  // When running outside ChromeOS, it should return the default downloads
  // directory for both MyFiles and Downloads for the primary user.
  EXPECT_EQ(GetDefaultDownloadDirectoryForLinuxChromeOS(),
            GetMyFilesFolder(*user));
  EXPECT_EQ(GetDefaultDownloadDirectoryForLinuxChromeOS(),
            GetDownloadsFolder(*user));

  // When running inside ChromeOS, it should return /home/chronos/u-{hash}/
  // MyFiles.
  base::test::ScopedRunningOnChromeOS running_on_chromeos;
  EXPECT_EQ(user_dir.Append("MyFiles"), GetMyFilesFolder(*user));
  EXPECT_EQ(user_dir.Append("MyFiles").Append("Downloads"),
            GetDownloadsFolder(*user));

  // Mount the volume to test the return from mount_points.
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      GetDownloadsMountPointName(*user), storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), base::FilePath("/media/test/MyFiles"));
  EXPECT_EQ(base::FilePath("/media/test/MyFiles"), GetMyFilesFolder(*user));
  EXPECT_EQ(base::FilePath("/media/test/MyFiles/Downloads"),
            GetDownloadsFolder(*user));
}

TEST_F(FileManagerUserPathUtilTest, GetMountPointNames) {
  const user_manager::User* user = LogIn();
  ASSERT_TRUE(user);
  // The fake username hash contains '@', which should be escaped.
  ASSERT_EQ("user@example.com-hash", user->username_hash());

  EXPECT_EQ("Downloads-user%40example.com-hash",
            GetDownloadsMountPointName(*user));
  EXPECT_EQ("ShareCache-user%40example.com-hash",
            GetShareCacheMountPointName(*user));
}

}  // namespace
}  // namespace file_manager::util
