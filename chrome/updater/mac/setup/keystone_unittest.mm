// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/keystone.h"

#import <Foundation/Foundation.h>

#include <vector>

#include "base/apple/foundation_util.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/registration_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

class KeystoneTest : public testing::Test {
 public:
  ~KeystoneTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_keystone_dir_.CreateUniqueTempDir());

    base::FilePath ticket_path =
        temp_keystone_dir_.GetPath().AppendASCII("TicketStore");
    ASSERT_TRUE(base::CreateDirectory(ticket_path));

    base::FilePath test_data_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
    test_data_path = test_data_path.AppendASCII("updater");

    ASSERT_TRUE(base::CopyFile(
        test_data_path.AppendASCII("Keystone.legacy.ticketstore"),
        ticket_path.AppendASCII("Keystone.ticketstore")));
    ASSERT_TRUE(base::CopyFile(
        test_data_path.AppendASCII("CountingMetrics.plist"),
        temp_keystone_dir_.GetPath().AppendASCII("CountingMetrics.plist")));
  }

 protected:
  base::ScopedTempDir temp_keystone_dir_;
};

TEST_F(KeystoneTest, CreateEmptyPlistFile) {
  constexpr int kPermissionsMask = base::FILE_PERMISSION_READ_BY_USER |
                                   base::FILE_PERMISSION_WRITE_BY_USER |
                                   base::FILE_PERMISSION_READ_BY_GROUP |
                                   base::FILE_PERMISSION_READ_BY_OTHERS;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Verify plist file is created if not present.
  const base::FilePath plist_path =
      temp_dir.GetPath().AppendASCII("empty.plist");
  EXPECT_TRUE(CreateEmptyPlistFile(plist_path));
  EXPECT_TRUE(base::PathExists(plist_path));
  int mode = 0;
  EXPECT_TRUE(base::GetPosixFilePermissions(plist_path, &mode));
  EXPECT_EQ(mode, kPermissionsMask);

  {
    // Verify the plist is not re-created when contents didn't change.
    base::Time previous_mtime = base::Time::Now() - base::Days(1);
    EXPECT_TRUE(base::TouchFile(plist_path, previous_mtime, previous_mtime));
    EXPECT_TRUE(CreateEmptyPlistFile(plist_path));
    base::File::Info info;
    EXPECT_TRUE(base::GetFileInfo(plist_path, &info));
    EXPECT_EQ(info.last_modified, previous_mtime);
  }

  @autoreleasepool {
    // Verify the plist is re-created when contents needs update.
    NSURL* const url = base::apple::FilePathToNSURL(plist_path);
    EXPECT_TRUE([@{@"foo" : @2} writeToURL:url error:nil]);
    base::Time previous_mtime = base::Time::Now() - base::Days(1);
    EXPECT_TRUE(base::TouchFile(plist_path, previous_mtime, previous_mtime));
    EXPECT_TRUE(CreateEmptyPlistFile(plist_path));
    base::File::Info info;
    EXPECT_TRUE(base::GetFileInfo(plist_path, &info));
    EXPECT_NE(info.last_modified, previous_mtime);
    mode = 0;
    EXPECT_TRUE(base::GetPosixFilePermissions(plist_path, &mode));
    EXPECT_EQ(mode, kPermissionsMask);
  }
}

TEST_F(KeystoneTest, MigrateKeystoneApps) {
  std::vector<RegistrationRequest> registration_requests;
  MigrateKeystoneApps(
      temp_keystone_dir_.GetPath(),
      base::BindLambdaForTesting(
          [&registration_requests](const RegistrationRequest& request) {
            registration_requests.push_back(request);
          }));

  EXPECT_EQ(registration_requests.size(), 4u);

  EXPECT_EQ(registration_requests[0].app_id, "com.chromium.corruptedapp");
  EXPECT_TRUE(registration_requests[0].brand_code.empty());
  EXPECT_TRUE(registration_requests[0].brand_path.empty());
  EXPECT_EQ(registration_requests[0].ap, "canary");
  EXPECT_EQ(registration_requests[0].version, base::Version("1.2.1"));
  EXPECT_EQ(registration_requests[0].existence_checker_path,
            base::FilePath("/"));
  EXPECT_FALSE(registration_requests[0].dla);   // Value is too big.
  EXPECT_FALSE(registration_requests[0].dlrc);  // Value is too small.

  EXPECT_EQ(registration_requests[1].app_id, "com.chromium.popularapp");
  EXPECT_TRUE(registration_requests[1].brand_code.empty());
  EXPECT_EQ(registration_requests[1].brand_path, base::FilePath("/"));
  EXPECT_EQ(registration_requests[1].ap, "GOOG");
  EXPECT_EQ(registration_requests[1].version,
            base::Version("101.100.1000.9999"));
  EXPECT_EQ(registration_requests[1].existence_checker_path,
            base::FilePath("/"));
  EXPECT_EQ(registration_requests[1].cohort, "TestCohort");
  EXPECT_EQ(registration_requests[1].cohort_hint, "TestCohortHint");
  EXPECT_EQ(registration_requests[1].cohort_name, "TestCohortName");
  EXPECT_EQ(registration_requests[1].dla.value(), 5921);
  EXPECT_EQ(registration_requests[1].dlrc.value(), 5922);

  EXPECT_EQ(registration_requests[2].app_id, "com.chromium.kipple");
  EXPECT_TRUE(registration_requests[2].brand_path.empty());
  EXPECT_EQ(registration_requests[2].existence_checker_path,
            base::FilePath("/"));
  EXPECT_FALSE(registration_requests[2].dla);   // No data.
  EXPECT_FALSE(registration_requests[2].dlrc);  // String value is ignored.

  EXPECT_EQ(registration_requests[3].app_id, "com.chromium.nonexistapp");
  EXPECT_TRUE(registration_requests[3].brand_path.empty());
}

}  // namespace updater
