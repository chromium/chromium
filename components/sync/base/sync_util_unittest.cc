// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_util.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "components/sync/base/command_line_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

}  // namespace syncer
