// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/keystone.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/bind.h"
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
