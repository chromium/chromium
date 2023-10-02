// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/command_line_policy_provider.h"

#include <memory>

#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace policy {

namespace {

void VerifyPolicyProvider(ConfigurationPolicyProvider* provider) {
  const base::Value* policy_value =
      provider->policies()
          .Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .GetValue("policy", base::Value::Type::INTEGER);
  ASSERT_TRUE(policy_value);
  ASSERT_TRUE(policy_value->is_int());
  EXPECT_EQ(10, policy_value->GetInt());
}

}  // namespace

class CommandLinePolicyProviderTest : public ::testing::Test {
 public:
  CommandLinePolicyProviderTest() {
    command_line_.AppendSwitchASCII(switches::kChromePolicy,
                                    R"({"policy":10})");
  }

  std::unique_ptr<CommandLinePolicyProvider> CreatePolicyProvider() {
    return CommandLinePolicyProvider::CreateForTesting(command_line_);
  }

  std::unique_ptr<CommandLinePolicyProvider> CreatePolicyProviderWithCheck(
      version_info::Channel channel) {
    return CommandLinePolicyProvider::CreateIfAllowed(command_line_, channel);
  }

  base::CommandLine* command_line() { return &command_line_; }

 private:
  base::CommandLine command_line_{base::CommandLine::NO_PROGRAM};
};

TEST_F(CommandLinePolicyProviderTest, LoadAndRefresh) {
  std::unique_ptr<CommandLinePolicyProvider> policy_provider =
      CreatePolicyProvider();
  VerifyPolicyProvider(policy_provider.get());

  policy_provider->RefreshPolicies(PolicyFetchReason::kTest);
  VerifyPolicyProvider(policy_provider.get());
}

TEST_F(CommandLinePolicyProviderTest, Creator) {
  version_info::Channel channels[] = {
      version_info::Channel::UNKNOWN, version_info::Channel::CANARY,
      version_info::Channel::DEV, version_info::Channel::BETA,
      version_info::Channel::STABLE};
  for (auto channel : channels) {
    bool is_created = false;
#if BUILDFLAG(IS_ANDROID)
    is_created = channel != version_info::Channel::BETA &&
                 channel != version_info::Channel::STABLE &&
                 base::android::BuildInfo::GetInstance()->is_debug_android();
#endif  // BUILDFLAG(IS_ANDROID)
    auto policy_provider = CreatePolicyProviderWithCheck(channel);
    if (is_created)
      EXPECT_TRUE(policy_provider);
    else
      EXPECT_FALSE(policy_provider);
  }
}

}  // namespace policy
