// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/client_filterable_state.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/variations_seed_store.h"
#include "components/variations/variations_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

struct PlatformOverrideTestParams {
  std::string test_name;
  std::string_view platform_switch;
  Study::Platform expected_platform;
};

class PlatformOverrideTest
    : public ::testing::TestWithParam<PlatformOverrideTestParams> {};

INSTANTIATE_TEST_SUITE_P(
    ClientFilterableStateTest,
    PlatformOverrideTest,
    ::testing::Values(
        PlatformOverrideTestParams{
            .test_name = "Android",
            .platform_switch = "android",
            .expected_platform = Study::PLATFORM_ANDROID},
        PlatformOverrideTestParams{
            .test_name = "AndroidWebView",
            .platform_switch = "android_webview",
            .expected_platform = Study::PLATFORM_ANDROID_WEBVIEW},
        PlatformOverrideTestParams{
            .test_name = "ChromeOS",
            .platform_switch = "chromeos",
            .expected_platform = Study::PLATFORM_CHROMEOS},
        PlatformOverrideTestParams{
            .test_name = "Fuchsia",
            .platform_switch = "fuchsia",
            .expected_platform = Study::PLATFORM_FUCHSIA},
        PlatformOverrideTestParams{.test_name = "Linux",
                                   .platform_switch = "linux",
                                   .expected_platform = Study::PLATFORM_LINUX},
        PlatformOverrideTestParams{.test_name = "iOS",
                                   .platform_switch = "ios",
                                   .expected_platform = Study::PLATFORM_IOS},
        PlatformOverrideTestParams{.test_name = "Mac",
                                   .platform_switch = "mac",
                                   .expected_platform = Study::PLATFORM_MAC},
        PlatformOverrideTestParams{.test_name = "Windows",
                                   .platform_switch = "win",
                                   .expected_platform = Study::PLATFORM_WINDOWS}

        ),
    [](const ::testing::TestParamInfo<PlatformOverrideTestParams>& params) {
      return params.param.test_name;
    });

TEST_P(PlatformOverrideTest, RespectsFakePlatformSwitch) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kFakeVariationsPlatform, GetParam().platform_switch);

  EXPECT_EQ(ClientFilterableState::GetCurrentPlatform(),
            GetParam().expected_platform);
}

TEST(ClientFilterableStateTest, IgnoreInvalidFakePlatformSwitch) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kFakeVariationsPlatform, "not_a_platform");

  Study::Platform actual_platform;
#if BUILDFLAG(IS_WIN)
  actual_platform = Study::PLATFORM_WINDOWS;
#elif BUILDFLAG(IS_IOS)
  actual_platform = Study::PLATFORM_IOS;
#elif BUILDFLAG(IS_MAC)
  actual_platform = Study::PLATFORM_MAC;
#elif BUILDFLAG(IS_CHROMEOS)
  actual_platform = Study::PLATFORM_CHROMEOS;
#elif BUILDFLAG(IS_ANDROID)
  actual_platform = Study::PLATFORM_ANDROID;
#elif BUILDFLAG(IS_FUCHSIA)
  actual_platform = Study::PLATFORM_FUCHSIA;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_BSD) || BUILDFLAG(IS_SOLARIS)
  // Default BSD and SOLARIS to Linux to not break those builds, although these
  // platforms are not officially supported by Chrome.
  actual_platform = Study::PLATFORM_LINUX;
#else
#error Unknown platform
#endif
  EXPECT_EQ(ClientFilterableState::GetCurrentPlatform(), actual_platform);
}

TEST(ClientFilterableStateTest, IsEnterprise) {
  // Test, for non enterprise clients, is_enterprise_function_ is called once.
  ClientFilterableState client_non_enterprise;
  EXPECT_FALSE(client_non_enterprise.IsEnterprise());
  EXPECT_FALSE(client_non_enterprise.IsEnterprise());

  // Test, for enterprise clients, is_enterprise_function_ is called once.
  std::unique_ptr<ClientFilterableState> client_enterprise =
      ClientFilterableState::CreateWithIsEnterprise(
          base::BindOnce([] { return true; }));
  EXPECT_TRUE(client_enterprise->IsEnterprise());
  EXPECT_TRUE(client_enterprise->IsEnterprise());
}

TEST(ClientFilterableStateTest, GoogleGroups) {
  // Test that google_groups_function_ is called once.
  base::flat_set<uint64_t> expected_google_groups({1234, 5678});
  std::unique_ptr<ClientFilterableState> client =
      ClientFilterableState::CreateWithGoogleGroups(base::BindOnce(
          [] { return base::flat_set<uint64_t>({1234, 5678}); }));
  EXPECT_EQ(client->GoogleGroups(), expected_google_groups);
  EXPECT_EQ(client->GoogleGroups(), expected_google_groups);
}

TEST(ClientFilterableStateTest, GetHardwareManufacturer) {
  std::string manufacturer = ClientFilterableState::GetHardwareManufacturer();
#if BUILDFLAG(IS_ANDROID)
  // On Android, the value is not hardcoded, but it should not be empty.
  EXPECT_FALSE(manufacturer.empty());
#else
  // For all other platforms, we expect the empty string fallback.
  EXPECT_TRUE(manufacturer.empty());
#endif
}

TEST(ClientFilterableStateTest, EnterpriseGroups) {
  // Test that enterprise_groups_function_ is called once.
  base::flat_set<std::string> expected_enterprise_groups({"abcd", "efgh"});
  std::unique_ptr<ClientFilterableState> client =
      ClientFilterableState::CreateWithEnterpriseGroups(base::BindOnce(
          [] { return base::flat_set<std::string>({"abcd", "efgh"}); }));
  EXPECT_EQ(client->EnterpriseGroups(), expected_enterprise_groups);
  EXPECT_EQ(client->EnterpriseGroups(), expected_enterprise_groups);
}

}  // namespace
}  // namespace variations
