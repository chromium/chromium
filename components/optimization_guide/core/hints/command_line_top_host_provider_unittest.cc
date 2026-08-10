// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hints/command_line_top_host_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(CommandLineTopHostProviderTest, DoesNotCreateIfFlagNotEnabled) {
  ASSERT_FALSE(CommandLineTopHostProvider::CreateIfEnabled());
}

TEST(CommandLineTopHostProviderTest, DoesNotCreateIfSwitchEnabledButNoHosts) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      kFetchHintsOverrideSwitch);

  ASSERT_FALSE(CommandLineTopHostProvider::CreateIfEnabled());
}

TEST(CommandLineTopHostProviderTest, CreateIfFlagEnabledAndHasHosts) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kFetchHintsOverrideSwitch, "whatever.com");

  std::unique_ptr<CommandLineTopHostProvider> top_host_provider =
      CommandLineTopHostProvider::CreateIfEnabled();
  ASSERT_TRUE(top_host_provider);
}

TEST(CommandLineTopHostProviderTest,
     GetTopHostsMaxLessThanProvidedSizeReturnsEverything) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kFetchHintsOverrideSwitch, "whatever.com");

  std::unique_ptr<CommandLineTopHostProvider> top_host_provider =
      CommandLineTopHostProvider::CreateIfEnabled();
  ASSERT_TRUE(top_host_provider);
  std::vector<std::string> top_hosts = top_host_provider->GetTopHosts();
  EXPECT_EQ(1u, top_hosts.size());
  EXPECT_EQ("whatever.com", top_hosts[0]);
}

TEST(CommandLineTopHostProviderTest,
     GetTopHostsMaxGreaterThanTotalVectorSizeReturnsFirstN) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kFetchHintsOverrideSwitch, "whatever.com, whatever-2.com, ,");

  std::unique_ptr<CommandLineTopHostProvider> top_host_provider =
      CommandLineTopHostProvider::CreateIfEnabled();
  ASSERT_TRUE(top_host_provider);
  std::vector<std::string> top_hosts = top_host_provider->GetTopHosts();
  EXPECT_EQ(2u, top_hosts.size());
  EXPECT_EQ("whatever.com", top_hosts[0]);
  EXPECT_EQ("whatever-2.com", top_hosts[1]);
}

}  // namespace optimization_guide
