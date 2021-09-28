// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/version.h"
#include "chrome/updater/mac/setup/keystone.h"
#include "chrome/updater/registration_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(KeystoneTest, TicketsToMigrate_NoTickets) {
  std::vector<RegistrationRequest> out =
      internal::TicketsToMigrate("No tickets\n");
  ASSERT_EQ(out.size(), 0UL);
}

TEST(KeystoneTest, TicketsToMigrate_Empty) {
  std::vector<RegistrationRequest> out = internal::TicketsToMigrate("");
  ASSERT_EQ(out.size(), 0UL);
}

TEST(KeystoneTest, TicketsToMigrate_Garbled) {
  std::vector<RegistrationRequest> out =
      internal::TicketsToMigrate("some\nbogus\01nonsense\xffvalue\0\n\n");
  ASSERT_EQ(out.size(), 0UL);
}

TEST(KeystoneTest, TicketsToMigrate_Tickets) {
  std::vector<RegistrationRequest> out = internal::TicketsToMigrate(
      "<KSTicket:0x7fda48414e90\n"
      "\tproductID=com.google.Keystone\n"
      "\tversion=1.3.16.180\n"
      "\txc=<KSPathExistenceChecker:0x7fda48414350 "
      "path=/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle>\n"
      "\turl=https://tools.google.com/service/update2\n"
      "\tcreationDate=2019-12-16 17:18:45\n"
      "\tcohort=1:0:\n"
      "\tcohortName=Everyone\n"
      "\tticketVersion=1\n"
      ">\n"
      "<KSTicket:0x7fda484134a0\n"
      "\tproductID=com.google.chrome_remote_desktop\n"
      "\tversion=94.0.4606.27\n"
      "\txc=<KSPathExistenceChecker:0x7fda48413c30 "
      "path=/Library/LaunchAgents/org.chromium.chromoting.plist>\n"
      "\tserverType=Omaha\n"
      "\turl=https://tools.google.com/service/update2\n"
      "\tcreationDate=2020-12-14 20:31:58\n"
      "\tcohort=1:10ql:\n"
      "\tcohortName=Stable\n"
      "\tticketVersion=1\n"
      ">\n"
      "<KSTicket:0x7fda484151f0\n"
      "\tproductID=com.google.Chrome\n"
      "\tversion=93.0.4577.82\n"
      "\txc=<KSPathExistenceChecker:0x7fda48414d20 path=/Applications/Google "
      "Chrome.app>\n"
      "\tserverType=Omaha\n"
      "\turl=https://tools.google.com/service/update2\n"
      "\tcreationDate=2019-12-19 17:15:37\n"
      "\ttagPath=/Applications/Google Chrome.app/Contents/Info.plist\n"
      "\ttagKey=KSChannelID\n"
      "\tbrandPath=/Library/Google/Google Chrome Brand.plist\n"
      "\tbrandKey=KSBrandID\n"
      "\tversionPath=/Applications/Google Chrome.app/Contents/Info.plist\n"
      "\tversionKey=KSVersion\n"
      "\tcohort=1:1y5:119f@0.5\n"
      "\tcohortName=Stable\n"
      "\tticketVersion=1\n"
      ">\n"
      "<KSTicket:0x7fda48415700\n"
      "\tproductID=com.google.Chrome.canary\n"
      "\tversion=96.0.4656.0\n"
      "\txc=<KSPathExistenceChecker:0x7fda48415810 path=/Applications/Google "
      "Chrome Canary.app>\n"
      "\tserverType=Omaha\n"
      "\turl=https://tools.google.com/service/update2\n"
      "\tcreationDate=2020-11-16 20:11:41\n"
      "\ttag=canary\n"
      "\ttagPath=/Applications/Google Chrome Canary.app/Contents/Info.plist\n"
      "\ttagKey=KSChannelID\n"
      "\tversionPath=/Applications/Google Chrome "
      "Canary.app/Contents/Info.plist\n"
      "\tversionKey=KSVersion\n"
      "\tcohort=1:0:\n"
      "\tcohortName=Canary\n"
      "\tticketVersion=1\n"
      ">\n");
  ASSERT_EQ(out.size(), 4UL);

  EXPECT_EQ(out[0].app_id, "com.google.Keystone");
  EXPECT_EQ(out[0].brand_code, "");
  EXPECT_EQ(out[0].tag, "");
  EXPECT_EQ(out[0].version, base::Version("1.3.16.180"));
  EXPECT_EQ(
      out[0].existence_checker_path,
      base::FilePath(
          "/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle"));

  EXPECT_EQ(out[1].app_id, "com.google.chrome_remote_desktop");
  EXPECT_EQ(out[1].brand_code, "");
  EXPECT_EQ(out[1].tag, "");
  EXPECT_EQ(out[1].version, base::Version("94.0.4606.27"));
  EXPECT_EQ(
      out[1].existence_checker_path,
      base::FilePath("/Library/LaunchAgents/org.chromium.chromoting.plist"));

  EXPECT_EQ(out[2].app_id, "com.google.Chrome");
  EXPECT_EQ(out[2].brand_code, "");
  EXPECT_EQ(out[2].tag, "");
  EXPECT_EQ(out[2].version, base::Version("93.0.4577.82"));
  EXPECT_EQ(out[2].existence_checker_path,
            base::FilePath("/Applications/Google Chrome.app"));

  EXPECT_EQ(out[3].app_id, "com.google.Chrome.canary");
  EXPECT_EQ(out[3].brand_code, "");
  EXPECT_EQ(out[3].tag, "canary");
  EXPECT_EQ(out[3].version, base::Version("96.0.4656.0"));
  EXPECT_EQ(out[3].existence_checker_path,
            base::FilePath("/Applications/Google Chrome Canary.app"));
}

}  // namespace updater
