// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/command_line_top_host_provider.h"

#include "base/command_line.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(CommandLineTopHostProviderTest, DoesNotCreateIfFlagNotEnabled) {
  ASSERT_FALSE(CommandLineTopHostProvider::CreateIfEnabled());
}

TEST(CommandLineTopHostProviderTest, DoesNotCreateIfSwitchEnabledButNoHosts) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kFetchHintsOverride);

  ASSERT_FALSE(CommandLineTopHostProvider::CreateIfEnabled());
}

TEST(CommandLineTopHostProviderTest, CreateIfFlagEnabledAndHasHosts) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kFetchHintsOverride, "whatever.com");

  std::unique_ptr<CommandLineTopHostProvider> top_host_provider =
      CommandLineTopHostProvider::CreateIfEnabled();
  ASSERT_TRUE(top_host_provider);
}

TEST(CommandLineTopHostProviderTest,
     GetTopHostsMaxLessThanProvidedSizeReturnsEverything) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kFetchHintsOverride, "whatever.com");

  std::unique_ptr<CommandLineTopHostProvider> top_host_provider =
      CommandLineTopHostProvider::CreateIfEnabled();
  ASSERT_TRUE(top_host_provider);
  std::vector<std::string> top_hosts = top_host_provider->GetTopHosts();
  EXPECT_EQ(1ul, top_hosts.size());
  EXPECT_EQ("whatever.com", top_hosts[0]);
}

TEST(CommandLineTopHostProviderTest,
     GetTopHostsMaxGreaterThanTotalVectorSizeReturnsFirstN) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kFetchHintsOverride, "whatever.com,awesome.com");

  std::unique_ptr<CommandLineTopHostProvider> top_host_provider =
      CommandLineTopHostProvider::CreateIfEnabled();
  ASSERT_TRUE(top_host_provider);
  std::vector<std::string> top_hosts = top_host_provider->GetTopHosts();
  EXPECT_EQ(2u, top_hosts.size());
  EXPECT_EQ("whatever.com", top_hosts[0]);
  EXPECT_EQ("awesome.com", top_hosts[1]);
}

}  // namespace optimization_guide
