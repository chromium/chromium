// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/unwind_util.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/profiler/profiler_buildflags.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/version_info/channel.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::Return;

// For `RequestUnwindPrerequisitesInstallation` and
// `AreUnwindPrerequisitesAvailable`-related unit tests below.
class MockModuleUnwindPrerequisitesDelegate
    : public UnwindPrerequisitesDelegate {
 public:
  MOCK_METHOD(void,
              RequestInstallation,
              (version_info::Channel channel),
              (override));
  MOCK_METHOD(bool, AreAvailable, (version_info::Channel channel), (override));
};

TEST(UnwindPrerequisitesTest, RequestInstall) {
  struct {
    version_info::Channel channel;
    bool enable_feature_install_android_unwind_dfm;
    bool is_installation_expected;
  } test_cases[] = {
    {version_info::Channel::CANARY, false, false},
    {version_info::Channel::DEV, false, false},
    {version_info::Channel::BETA, false, false},
    {version_info::Channel::STABLE, false, false},
    {version_info::Channel::UNKNOWN, false, false},
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL) &&           \
    BUILDFLAG(ENABLE_ARM_CFI_TABLE) && defined(OFFICIAL_BUILD) && \
    BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {version_info::Channel::CANARY, true, true},
    {version_info::Channel::DEV, true, true},
    {version_info::Channel::BETA, true, true},
    {version_info::Channel::STABLE, true, true},
    {version_info::Channel::UNKNOWN, true, true},
#else
    {version_info::Channel::CANARY, true, false},
    {version_info::Channel::DEV, true, false},
    {version_info::Channel::BETA, true, false},
    {version_info::Channel::STABLE, true, false},
    {version_info::Channel::UNKNOWN, true, false},
#endif
  };

  for (const auto& test_case : test_cases) {
    base::test::ScopedFeatureList feature_list;
    if (test_case.enable_feature_install_android_unwind_dfm) {
      feature_list.InitAndEnableFeature(kInstallAndroidUnwindDfm);
    } else {
      feature_list.InitAndDisableFeature(kInstallAndroidUnwindDfm);
    }

    MockModuleUnwindPrerequisitesDelegate mock_delegate;
    EXPECT_CALL(mock_delegate, RequestInstallation(_))
        .Times(test_case.is_installation_expected ? 1 : 0);

    RequestUnwindPrerequisitesInstallation(test_case.channel, &mock_delegate);
  }
}

TEST(UnwindPrerequisitesDeathTest, CannotRequestInstallOutsideBrowser) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kProcessType, switches::kRendererProcess);
  MockModuleUnwindPrerequisitesDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, RequestInstallation(_)).Times(0);

  ASSERT_DEATH_IF_SUPPORTED(RequestUnwindPrerequisitesInstallation(
                                version_info::Channel::UNKNOWN, &mock_delegate),
                            "");
}

TEST(UnwindPrerequisitesTest, AreUnwindPrerequisitesAvailable) {
  MockModuleUnwindPrerequisitesDelegate true_mock_delegate;
  EXPECT_CALL(true_mock_delegate, AreAvailable(_)).WillRepeatedly(Return(true));

  MockModuleUnwindPrerequisitesDelegate false_mock_delegate;
  EXPECT_CALL(false_mock_delegate, AreAvailable(_))
      .WillRepeatedly(Return(false));

  struct {
    version_info::Channel channel;
    UnwindPrerequisitesDelegate* delegate;
    bool are_unwind_prerequisites_expected;
  } test_cases[] = {
    {version_info::Channel::CANARY, &true_mock_delegate, true},
    {version_info::Channel::DEV, &true_mock_delegate, true},
    {version_info::Channel::BETA, &true_mock_delegate, true},
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL) && \
    BUILDFLAG(ENABLE_ARM_CFI_TABLE)
    {version_info::Channel::CANARY, &false_mock_delegate, false},
    {version_info::Channel::DEV, &false_mock_delegate, false},
    {version_info::Channel::BETA, &false_mock_delegate, false},
    {version_info::Channel::STABLE, &false_mock_delegate, false},
    {version_info::Channel::UNKNOWN, &false_mock_delegate, false},
#if defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {version_info::Channel::STABLE, &true_mock_delegate, false},
    {version_info::Channel::UNKNOWN, &true_mock_delegate, false},
#else  // defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {version_info::Channel::STABLE, &true_mock_delegate, true},
    {version_info::Channel::UNKNOWN, &true_mock_delegate, true},
#endif  // defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else   // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL) &&
        // BUILDFLAG(ENABLE_ARM_CFI_TABLE)
    {version_info::Channel::CANARY, &false_mock_delegate, true},
    {version_info::Channel::DEV, &false_mock_delegate, true},
    {version_info::Channel::BETA, &false_mock_delegate, true},
    {version_info::Channel::STABLE, &true_mock_delegate, true},
    {version_info::Channel::STABLE, &false_mock_delegate, true},
    {version_info::Channel::UNKNOWN, &true_mock_delegate, true},
    {version_info::Channel::UNKNOWN, &false_mock_delegate, true},
#endif  // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL) &&
        // BUILDFLAG(ENABLE_ARM_CFI_TABLE)
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        AreUnwindPrerequisitesAvailable(test_case.channel, test_case.delegate),
        test_case.are_unwind_prerequisites_expected);
  }
}

}  // namespace
