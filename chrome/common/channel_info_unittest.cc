// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/test/base/scoped_channel_override.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace chrome {

namespace {

// A bucket of test parameters for ChannelInfoTest.
struct Param {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  Param(ScopedChannelOverride::Channel channel_override,
        const char* name_without_es,
        const char* name_with_es,
        version_info::Channel channel,
        bool is_extended_stable,
        const char* posix_data_dir_suffix)
      : channel_override(channel_override),
        channel_name_without_es(name_without_es),
        channel_name_with_es(name_with_es),
        channel(channel),
        is_extended_stable(is_extended_stable),
        posix_data_dir_suffix(posix_data_dir_suffix) {}
#else
  Param(const char* name_without_es,
        const char* name_with_es,
        version_info::Channel channel,
        bool is_extended_stable,
        const char* posix_data_dir_suffix)
      : channel_name_without_es(name_without_es),
        channel_name_with_es(name_with_es),
        channel(channel),
        is_extended_stable(is_extended_stable),
        posix_data_dir_suffix(posix_data_dir_suffix) {}
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Value to use in the test to override the default channel in branded builds.
  const ScopedChannelOverride::Channel channel_override;
#endif

  // Expected channel name when extended stable should not be surfaced.
  const char* const channel_name_without_es;

  // Expected channel name when extended stable should be surfaced.
  const char* const channel_name_with_es;

  // Expected channel value.
  const version_info::Channel channel;

  // Expected extended stable channel value.
  const bool is_extended_stable;

  // Suffix for User Data dir (only used for non-Mac Posix).
  const char* const posix_data_dir_suffix;
};

}  // namespace

// A value-parameterized test to facilitate testing the various channel info
// functions. Branded builds evaluate all tests once for each supported channel.
class ChannelInfoTest : public ::testing::TestWithParam<Param> {
 protected:
  ChannelInfoTest() = default;

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ScopedChannelOverride scoped_channel_override_{GetParam().channel_override};
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

TEST_P(ChannelInfoTest, GetVersionStringWithout) {
  const std::string channel_name = GetParam().channel_name_without_es;
  if (!channel_name.empty()) {
    EXPECT_THAT(GetVersionString(WithExtendedStable(false)),
                ::testing::EndsWith(channel_name));
  }
}

TEST_P(ChannelInfoTest, GetVersionStringWith) {
  const std::string channel_name = GetParam().channel_name_with_es;
  if (!channel_name.empty()) {
    EXPECT_THAT(GetVersionString(WithExtendedStable(true)),
                ::testing::EndsWith(channel_name));
  }
}

TEST_P(ChannelInfoTest, GetChannelNameWithout) {
  EXPECT_EQ(GetChannelName(WithExtendedStable(false)),
            GetParam().channel_name_without_es);
}

TEST_P(ChannelInfoTest, GetChannelNameWith) {
  EXPECT_EQ(GetChannelName(WithExtendedStable(true)),
            GetParam().channel_name_with_es);
}

TEST_P(ChannelInfoTest, GetChannel) {
  EXPECT_EQ(GetChannel(), GetParam().channel);
}

TEST_P(ChannelInfoTest, IsExtendedStableChannel) {
  EXPECT_EQ(IsExtendedStableChannel(), GetParam().is_extended_stable);
}

#if BUILDFLAG(IS_WIN)
#elif BUILDFLAG(IS_MAC)

TEST_P(ChannelInfoTest, GetChannelByName) {
  EXPECT_EQ(GetChannelByName(GetParam().channel_name_with_es),
            GetParam().channel);
}

#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_P(ChannelInfoTest, GetChannelSuffixForDataDir) {
  EXPECT_EQ(GetChannelSuffixForDataDir(), GetParam().posix_data_dir_suffix);
}

#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Instantiate the test suite once per supported channel in branded builds.
INSTANTIATE_TEST_SUITE_P(
    Stable,
    ChannelInfoTest,
    ::testing::Values(Param(ScopedChannelOverride::Channel::kStable,
                            "",
                            "",
                            version_info::Channel::STABLE,
                            /*is_extended_stable=*/false,
                            /*posix_data_dir_suffix=*/"")));
INSTANTIATE_TEST_SUITE_P(
    ExtendedStable,
    ChannelInfoTest,
    ::testing::Values(Param(ScopedChannelOverride::Channel::kExtendedStable,
                            "",
                            "extended",
                            version_info::Channel::STABLE,
                            /*is_extended_stable=*/true,
                            /*posix_data_dir_suffix=*/"")));
INSTANTIATE_TEST_SUITE_P(
    Beta,
    ChannelInfoTest,
    ::testing::Values(Param(ScopedChannelOverride::Channel::kBeta,
                            "beta",
                            "beta",
                            version_info::Channel::BETA,
                            /*is_extended_stable=*/false,
                            /*posix_data_dir_suffix=*/"-beta")));
INSTANTIATE_TEST_SUITE_P(
    Dev,
    ChannelInfoTest,
    ::testing::Values(Param(ScopedChannelOverride::Channel::kDev,
                            "dev",
                            "dev",
                            version_info::Channel::DEV,
                            /*is_extended_stable=*/false,
                            /*posix_data_dir_suffix=*/"-unstable")));
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
INSTANTIATE_TEST_SUITE_P(
    Canary,
    ChannelInfoTest,
    ::testing::Values(Param(ScopedChannelOverride::Channel::kCanary,
                            "canary",
                            "canary",
                            version_info::Channel::CANARY,
                            /*is_extended_stable=*/false,
                            /*posix_data_dir_suffix=*/"")));
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)  ||
        // BUILDFLAG(IS_LINUX)
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
INSTANTIATE_TEST_SUITE_P(
    Chromium,
    ChannelInfoTest,
    ::testing::Values(Param("",
                            "",
                            version_info::Channel::UNKNOWN,
                            /*is_extended_stable=*/false,
                            /*posix_data_dir_suffix=*/"")));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace chrome
