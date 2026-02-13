// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/core_unwinders.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
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

#if (defined(ARCH_CPU_ARMEL) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)) || \
    (defined(ARCH_CPU_ARM64) && BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS))
constexpr bool kUnwindingSupported = true;
#else
constexpr bool kUnwindingSupported = false;
#endif

#if defined(ARCH_CPU_ARMEL) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
constexpr bool kPrerequisitesRelyOnDfm = true;
#else
constexpr bool kPrerequisitesRelyOnDfm = false;
#endif

#if defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr bool kIsOfficialGoogleBuild = true;
#else
constexpr bool kIsOfficialGoogleBuild = false;
#endif

bool ExpectedAvailability(version_info::Channel channel,
                          bool are_prerequisites_installed) {
  if (!kUnwindingSupported) {
    return false;
  }
  if (kIsOfficialGoogleBuild) {
    if (channel != version_info::Channel::CANARY &&
        channel != version_info::Channel::DEV &&
        channel != version_info::Channel::BETA) {
      return false;
    }
  }
  if (kPrerequisitesRelyOnDfm) {
    return are_prerequisites_installed;
  }
  return true;
}

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
  for (auto channel :
       {version_info::Channel::CANARY, version_info::Channel::DEV,
        version_info::Channel::BETA, version_info::Channel::STABLE,
        version_info::Channel::UNKNOWN}) {
    for (bool enable_feature : {false, true}) {
      for (bool are_available_initially : {false, true}) {
        base::test::ScopedFeatureList feature_list;
        if (enable_feature) {
          feature_list.InitAndEnableFeature(kInstallAndroidUnwindDfm);
        } else {
          feature_list.InitAndDisableFeature(kInstallAndroidUnwindDfm);
        }

        MockModuleUnwindPrerequisitesDelegate mock_delegate;
        EXPECT_CALL(mock_delegate, AreAvailable(channel))
            .WillRepeatedly(Return(are_available_initially));

        bool installation_expected = false;
        if (!ExpectedAvailability(channel, are_available_initially)) {
          installation_expected = kPrerequisitesRelyOnDfm &&
                                  kIsOfficialGoogleBuild && enable_feature;
        }

        EXPECT_CALL(mock_delegate, RequestInstallation(_))
            .Times(installation_expected ? 1 : 0);

        RequestUnwindPrerequisitesInstallation(channel, &mock_delegate);
      }
    }
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
  for (auto channel :
       {version_info::Channel::CANARY, version_info::Channel::DEV,
        version_info::Channel::BETA, version_info::Channel::STABLE,
        version_info::Channel::UNKNOWN}) {
    for (bool are_available : {false, true}) {
      MockModuleUnwindPrerequisitesDelegate mock_delegate;
      EXPECT_CALL(mock_delegate, AreAvailable(channel))
          .WillRepeatedly(Return(are_available));

      bool expected = ExpectedAvailability(channel, are_available);
      EXPECT_EQ(AreUnwindPrerequisitesAvailable(channel, &mock_delegate),
                expected)
          << "Failed for channel " << static_cast<int>(channel) << " and "
          << (are_available ? "true" : "false") << " delegate";
    }
  }
}

}  // namespace
