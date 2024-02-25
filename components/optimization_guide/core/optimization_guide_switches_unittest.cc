// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_switches.h"

#include <optional>

#include "base/base64.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "components/optimization_guide/proto/hints.pb.h"
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

TEST(OptimizationGuideSwitchesTest, ParseComponentConfigFromCommandLine) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  encoded_config = base::Base64Encode(encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kHintsProtoOverride,
                                                            encoded_config);

  std::unique_ptr<optimization_guide::proto::Configuration> parsed_config =
      ParseComponentConfigFromCommandLine();

  EXPECT_EQ(1, parsed_config->hints_size());
  EXPECT_EQ("somedomain.org", parsed_config->hints(0).key());
}

TEST(OptimizationGuideSwitchesTest,
     ParseComponentConfigFromCommandLineNotAProto) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kHintsProtoOverride,
                                                            "not-a-proto");

  std::unique_ptr<optimization_guide::proto::Configuration> parsed_config =
      ParseComponentConfigFromCommandLine();

  EXPECT_EQ(nullptr, parsed_config);
}

TEST(OptimizationGuideSwitchesTest,
     ParseComponentConfigFromCommandLineSwitchNotSet) {
  std::unique_ptr<optimization_guide::proto::Configuration> parsed_config =
      ParseComponentConfigFromCommandLine();

  EXPECT_EQ(nullptr, parsed_config);
}

TEST(OptimizationGuideSwitchesTest,
     ParseComponentConfigFromCommandLineNotAConfiguration) {
  optimization_guide::proto::HostInfo host_info;
  host_info.set_host("whatever.com");
  std::string encoded_proto;
  host_info.SerializeToString(&encoded_proto);
  encoded_proto = base::Base64Encode(encoded_proto);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kHintsProtoOverride,
                                                            encoded_proto);

  std::unique_ptr<optimization_guide::proto::Configuration> parsed_config =
      ParseComponentConfigFromCommandLine();

  EXPECT_EQ(nullptr, parsed_config);
}


#endif

}  // namespace switches
}  // namespace optimization_guide
