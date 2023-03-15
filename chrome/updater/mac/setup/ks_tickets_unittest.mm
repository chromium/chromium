// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/setup/ks_tickets.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(KSTicketsTest, Decode) {
  @autoreleasepool {
    base::FilePath test_data_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
    NSDictionary<NSString*, KSTicket*>* store = [KSTicketStore
        readStoreWithPath:base::SysUTF8ToNSString(
                              test_data_path
                                  .Append(FILE_PATH_LITERAL("updater"))
                                  .Append(
                                      FILE_PATH_LITERAL("Keystone.ticketstore"))
                                  .AsUTF8Unsafe())];

    ASSERT_TRUE(store);
    EXPECT_EQ([store count], 5ul);
    EXPECT_EQ(
        base::SysNSStringToUTF8(
            [[store objectForKey:@"com.google.chrome.canary"] description]),
        "<KSTicket:0x222222222222\n"
        "\tproductID=com.google.Chrome.canary\n"
        "\tversion=96.0.4662.0\n"
        "\txc=<KSPathExistenceChecker:0x222222222222 "
        "path=/Applications/Google Chrome Canary.app>\n"
        "\tserverType=Omaha\n"
        "\turl=https://tools.google.com/service/update2\n"
        "\tcreationDate=2020-11-16 20:11:41\n"
        "\ttag=canary\n"
        "\ttagPath=/Applications/Google Chrome Canary.app/Contents/Info.plist\n"
        "\ttagKey=KSChannelID\n"
        "\tversionPath="
        "/Applications/Google Chrome Canary.app/Contents/Info.plist\n"
        "\tversionKey=KSVersion\n"
        "\tcohort=1:0:\n"
        "\tcohortName=Canary\n"
        "\tticketVersion=1\n"
        ">");
    EXPECT_EQ(
        base::SysNSStringToUTF8([[store
            objectForKey:@"com.google.chrome_remote_desktop"] description]),
        "<KSTicket:0x222222222222\n"
        "\tproductID=com.google.chrome_remote_desktop\n"
        "\tversion=94.0.4606.27\n"
        "\txc=<KSPathExistenceChecker:0x222222222222 "
        "path=/Library/LaunchAgents/org.chromium.chromoting.plist>\n"
        "\tserverType=Omaha\n"
        "\turl=https://tools.google.com/service/update2\n"
        "\tcreationDate=2020-12-14 20:31:58\n"
        "\tcohort=1:10ql:\n"
        "\tcohortName=Stable\n"
        "\tticketVersion=1\n"
        ">");

    EXPECT_EQ(base::SysNSStringToUTF8([[store
                  objectForKey:@"com.google.secureconnect"] description]),
              "<KSTicket:0x222222222222\n"
              "\tproductID=com.google.SecureConnect\n"
              "\tversion=2.1.8\n"
              "\txc=<KSPathExistenceChecker:0x222222222222 "
              "path=/Library/Application Support/Google/Endpoint "
              "Verification/ApiHelper>\n"
              "\tserverType=Omaha\n"
              "\turl=https://tools.google.com/service/update2\n"
              "\tcreationDate=2019-12-16 17:25:15\n"
              "\tcohort=1::\n"
              "\tticketVersion=1\n"
              ">");

    EXPECT_EQ(
        base::SysNSStringToUTF8(
            [[store objectForKey:@"com.google.chrome"] description]),
        "<KSTicket:0x222222222222\n"
        "\tproductID=com.google.Chrome\n"
        "\tversion=94.0.4606.71\n"
        "\txc=<KSPathExistenceChecker:0x222222222222 "
        "path=/Applications/Google Chrome.app>\n"
        "\tserverType=Omaha\n"
        "\turl=https://tools.google.com/service/update2\n"
        "\tcreationDate=2019-12-19 17:15:37\n"
        "\ttagPath=/Applications/Google Chrome.app/Contents/Info.plist\n"
        "\ttagKey=KSChannelID\n"
        "\tbrandPath=/Library/Google/Google Chrome Brand.plist\n"
        "\tbrandKey=KSBrandID\n"
        "\tversionPath=/Applications/Google Chrome.app/Contents/Info.plist\n"
        "\tversionKey=KSVersion\n"
        "\tcohort=1:1y5:\n"
        "\tcohortName=Stable\n"
        "\tticketVersion=1\n"
        ">");

    EXPECT_EQ(base::SysNSStringToUTF8(
                  [[store objectForKey:@"com.google.keystone"] description]),
              "<KSTicket:0x222222222222\n"
              "\tproductID=com.google.Keystone\n"
              "\tversion=1.3.16.180\n"
              "\txc=<KSPathExistenceChecker:0x222222222222 "
              "path=/Library/Google/GoogleSoftwareUpdate/"
              "GoogleSoftwareUpdate.bundle>\n"
              "\turl=https://tools.google.com/service/update2\n"
              "\tcreationDate=2019-12-16 17:18:45\n"
              "\tcohort=1:0:\n"
              "\tcohortName=Everyone\n"
              "\tticketVersion=1\n"
              ">");
  }
}

TEST(KSTicketsTest, DecodeWithLegacyChecker) {
  @autoreleasepool {
    base::FilePath test_data_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
    NSDictionary<NSString*, KSTicket*>* store = [KSTicketStore
        readStoreWithPath:
            base::SysUTF8ToNSString(
                test_data_path.Append(FILE_PATH_LITERAL("updater"))
                    .Append(FILE_PATH_LITERAL(
                        "Keystone.withlaunchservicechecker.ticketstore"))
                    .AsUTF8Unsafe())];

    ASSERT_TRUE(store);
    EXPECT_EQ([store count], 4ul);
    EXPECT_EQ(
        base::SysNSStringToUTF8(
            [[store objectForKey:@"com.google.chrome.canary"] description]),
        "<KSTicket:0x222222222222\n"
        "\tproductID=com.google.Chrome.canary\n"
        "\tversion=113.0.5639.0\n"
        "\txc=<KSPathExistenceChecker:0x222222222222 "
        "path=/Applications/Google Chrome Canary.app>\n"
        "\tserverType=Omaha\n"
        "\turl=https://tools.google.com/service/update2\n"
        "\tcreationDate=2011-05-19 19:35:16\n"
        "\ttag=universal-canary\n"
        "\ttagPath=/Applications/Google Chrome Canary.app/Contents/Info.plist\n"
        "\ttagKey=KSChannelID\n"
        "\tversionPath="
        "/Applications/Google Chrome Canary.app/Contents/Info.plist\n"
        "\tversionKey=KSVersion\n"
        "\tcohort=1:0:1ihf@0.05\n"
        "\tcohortName=Canary\n"
        "\tticketVersion=1\n"
        ">");
    EXPECT_EQ(
        base::SysNSStringToUTF8(
            [[store objectForKey:@"com.google.chrome"] description]),
        "<KSTicket:0x222222222222\n"
        "\tproductID=com.google.Chrome\n"
        "\tversion=112.0.5615.20\n"
        "\txc=<KSPathExistenceChecker:0x222222222222 "
        "path=/Applications/Google Chrome.app>\n"
        "\tserverType=Omaha\n"
        "\turl=https://tools.google.com/service/update2\n"
        "\tcreationDate=2011-05-19 19:34:37\n"
        "\ttag=universal-dev\n"
        "\ttagPath=/Applications/Google Chrome.app/Contents/Info.plist\n"
        "\ttagKey=KSChannelID\n"
        "\tversionPath=/Applications/Google Chrome.app/Contents/Info.plist\n"
        "\tversionKey=KSVersion\n"
        "\tcohort=1:0:\n"
        "\tcohortName=Dev\n"
        "\tticketVersion=1\n"
        ">");
    EXPECT_EQ(base::SysNSStringToUTF8(
                  [[store objectForKey:@"com.google.drivefs"] description]),
              "<KSTicket:0x222222222222\n"
              "\tproductID=com.google.drivefs\n"
              "\tversion=70.0.2.0\n"
              "\txc=<KSPathExistenceChecker:0x222222222222 "
              "path=/Applications/Google Drive.app>\n"
              "\tserverType=Omaha\n"
              "\turl=https://tools.google.com/service/update2\n"
              "\tcreationDate=2021-05-27 14:47:38\n"
              "\ttag=No ticket for com.google.drivefs\n"
              "\tcohort=1:0/og3:\n"
              "\tcohortName=Stable-50-Percent\n"
              "\tticketVersion=1\n"
              ">");
    EXPECT_EQ(base::SysNSStringToUTF8([[store
                  objectForKey:@"com.google.macphotouploader"] description]),
              "<KSTicket:0x222222222222\n"
              "\tproductID=com.google.macphotouploader\n"
              "\tversion=1.5.0.1425\n"
              "\txc=(null)\n"
              "\turl=https://tools.google.com/service/update2\n"
              "\tcreationDate=2009-01-29 03:59:57\n"
              "\tticketVersion=0\n"
              ">");
  }
}

}  // namespace updater
