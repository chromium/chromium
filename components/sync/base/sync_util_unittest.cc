// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_util.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/sync/base/command_line_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace syncer {

TEST(SyncUtilTest, GetSyncServiceURLWithoutCommandLineSwitch) {
  // If the command line is not set the url is one of two constants chosen based
  // on the channel (e.g. beta).
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  std::string url =
      GetSyncServiceURL(command_line, version_info::Channel::BETA).spec();
  ASSERT_TRUE(internal::kSyncServerUrl == url ||
              internal::kSyncDevServerUrl == url);
}

TEST(SyncUtilTest, GetSyncServiceURLWithCommandLineSwitch) {
  // See that we can set the URL via the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(kSyncServiceURL, "https://foo/bar");
  ASSERT_EQ(
      "https://foo/bar",
      GetSyncServiceURL(command_line, version_info::Channel::UNKNOWN).spec());
}

TEST(SyncUtilTest, GetSyncServiceURLWithBadCommandLineSwitch) {
  // If the command line value is not a valid url it is ignored.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(kSyncServiceURL, "invalid_url");
  std::string url =
      GetSyncServiceURL(command_line, version_info::Channel::UNKNOWN).spec();
  ASSERT_TRUE(internal::kSyncServerUrl == url ||
              internal::kSyncDevServerUrl == url);
}

TEST(SyncUtilTest, FormatUserAgentForSync) {
  std::string user_agent =
      internal::FormatUserAgentForSync("TEST", version_info::Channel::UNKNOWN);
  ASSERT_TRUE(base::StartsWith(user_agent, "Chrome TEST",
                               base::CompareCase::SENSITIVE));
}

// Verifies that `IsIgsaSyncUserAgent()` matches the `"iGSA"` user agent prefix
// used by `//ios/web_view` while rejecting Chrome, lowercase, non-prefix, and
// empty user agents.
TEST(SyncUtilTest, IsIgsaSyncUserAgent) {
  EXPECT_TRUE(IsIgsaSyncUserAgent(
      "iGSA IOS-PHONE 158.0.8127.86 (0000000000000000-stable)"));
  EXPECT_TRUE(IsIgsaSyncUserAgent(
      "iGSA IOS-TABLET 158.0.8127.86 (0000000000000000-stable)"));
  EXPECT_TRUE(IsIgsaSyncUserAgent("iGSA/1.0"));
  EXPECT_FALSE(IsIgsaSyncUserAgent(
      "Chrome IOS-PHONE 158.0.8127.86 (0000000000000000-stable)"));
  EXPECT_FALSE(
      IsIgsaSyncUserAgent(MakeUserAgentForSync(version_info::Channel::STABLE)));
  EXPECT_FALSE(IsIgsaSyncUserAgent("igsa IOS-PHONE 158.0.8127.86"));
  EXPECT_FALSE(IsIgsaSyncUserAgent("Mozilla/5.0 iGSA"));
  EXPECT_FALSE(IsIgsaSyncUserAgent(""));
}

#if BUILDFLAG(IS_ANDROID)
TEST(SyncUtilTest, MakeUserAgentForSyncAndroidDesktop) {
  if (base::android::device_info::is_automotive()) {
    // The "is_automotive" flag is stronger than "is_desktop", see
    // GetDeviceFormFactor(), so the set_is_desktop_for_testing(true) override
    // does not work on automotive.
    GTEST_SKIP() << "This test is not applicable to automotive.";
  }

  base::android::device_info::set_is_desktop_for_testing(true);
  std::string user_agent = MakeUserAgentForSync(version_info::Channel::UNKNOWN);
  EXPECT_TRUE(base::StartsWith(user_agent, "Chrome ANDROID-DESKTOP",
                               base::CompareCase::SENSITIVE));
  base::android::device_info::reset_is_desktop_for_testing();
}
#endif

}  // namespace syncer
