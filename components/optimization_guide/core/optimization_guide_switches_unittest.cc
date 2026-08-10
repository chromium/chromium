// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_switches.h"

#include <optional>

#include "base/command_line.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace switches {

#if !BUILDFLAG(IS_WIN)

TEST(OptimizationGuideSwitchesTest, ParseHintsFetchOverrideFromCommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kFetchHintsOverride,
                                                            "whatever.com");

  std::optional<std::vector<std::string>> parsed_hosts =
      ParseHintsFetchOverrideFromCommandLine();

  EXPECT_TRUE(parsed_hosts.has_value());
  EXPECT_EQ(1ul, parsed_hosts.value().size());
  EXPECT_EQ("whatever.com", parsed_hosts.value()[0]);
}

TEST(OptimizationGuideSwitchesTest,
     ParseHintsFetchOverrideFromCommandLineMultipleHosts) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kFetchHintsOverride, "whatever.com, whatever-2.com, ,");

  std::optional<std::vector<std::string>> parsed_hosts =
      ParseHintsFetchOverrideFromCommandLine();

  EXPECT_TRUE(parsed_hosts.has_value());
  EXPECT_EQ(2ul, parsed_hosts.value().size());
  EXPECT_EQ("whatever.com", parsed_hosts.value()[0]);
  EXPECT_EQ("whatever-2.com", parsed_hosts.value()[1]);
}

TEST(OptimizationGuideSwitchesTest,
     ParseHintsFetchOverrideFromCommandLineNoSwitch) {
  std::optional<std::vector<std::string>> parsed_hosts =
      ParseHintsFetchOverrideFromCommandLine();

  EXPECT_FALSE(parsed_hosts.has_value());
}

#endif

}  // namespace switches
}  // namespace optimization_guide
