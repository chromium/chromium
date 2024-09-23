// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/additional_parameters.h"

#include <optional>
#include <string_view>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "build/build_config.h"
#include "chrome/install_static/install_util.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

class AdditionalParametersTest : public ::testing::Test {
 protected:
  AdditionalParametersTest() = default;

  static void CreateKey() {
    ASSERT_TRUE(
        base::win::RegKey(HKEY_CURRENT_USER,
                          install_static::GetClientStateKeyPath().c_str(),
                          KEY_SET_VALUE)
            .Valid());
  }

  static void SetAp(const wchar_t* value) {
    ASSERT_EQ(base::win::RegKey(HKEY_CURRENT_USER,
                                install_static::GetClientStateKeyPath().c_str(),
                                KEY_WOW64_32KEY | KEY_SET_VALUE)
                  .WriteValue(L"ap", value),
              ERROR_SUCCESS);
  }

  static std::optional<std::wstring> GetAp() {
    std::wstring value;
    if (base::win::RegKey(HKEY_CURRENT_USER,
                          install_static::GetClientStateKeyPath().c_str(),
                          KEY_WOW64_32KEY | KEY_QUERY_VALUE)
            .ReadValue(L"ap", &value) == ERROR_SUCCESS) {
      return std::move(value);
    }
    return std::nullopt;
  }

  // ::testing::Test:
  void SetUp() override {
    ASSERT_FALSE(install_static::IsSystemInstall())
        << "system-level not supported";
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
};

TEST_F(AdditionalParametersTest, GetStatsDefaultNoKey) {
  AdditionalParameters ap;
  EXPECT_EQ(ap.GetStatsDefault(), 0);
}

TEST_F(AdditionalParametersTest, GetStatsDefaultNoValue) {
  ASSERT_NO_FATAL_FAILURE(CreateKey());
  AdditionalParameters ap;
  EXPECT_EQ(ap.GetStatsDefault(), 0);
}

TEST_F(AdditionalParametersTest, GetStatsDefault) {
  static constexpr struct {
    const wchar_t* ap_value;
    wchar_t expected;
  } kExpectations[] = {
      {L"", 0},
      {L"somevaluebutnothing", 0},
      {L"-statsdef", 0},
      {L"-statsdef_", 0},
      {L"statsdef_0", 0},
      {L"-statsdef_0", L'0'},
      {L"-statsdef_1", L'1'},
      {L"-statsdef_1000", L'1'},
      {L"blahblah-statsdef_1-blah", L'1'},
  };
  for (const auto& expectation : kExpectations) {
    ASSERT_NO_FATAL_FAILURE(SetAp(expectation.ap_value));
    AdditionalParameters ap;
    EXPECT_EQ(ap.GetStatsDefault(), expectation.expected);
  }
}

TEST_F(AdditionalParametersTest, SetFullSuffixNoKey) {
  {
    AdditionalParameters ap;
    EXPECT_FALSE(ap.SetFullSuffix(false));
    EXPECT_EQ(GetAp(), std::nullopt);
  }

  {
    AdditionalParameters ap;
    EXPECT_TRUE(ap.SetFullSuffix(true));
    ASSERT_TRUE(ap.Commit());
    EXPECT_EQ(GetAp(), std::optional<std::wstring>(L"-full"));
  }
}

TEST_F(AdditionalParametersTest, SetFullSuffixNoValue) {
  ASSERT_NO_FATAL_FAILURE(CreateKey());
  {
    AdditionalParameters ap;
    EXPECT_FALSE(ap.SetFullSuffix(false));
    EXPECT_EQ(GetAp(), std::nullopt);
  }

  {
    AdditionalParameters ap;
    EXPECT_TRUE(ap.SetFullSuffix(true));
    ASSERT_TRUE(ap.Commit());
    EXPECT_EQ(GetAp(), std::optional<std::wstring>(L"-full"));
  }
}

TEST_F(AdditionalParametersTest, SetFullSuffix) {
  static constexpr struct {
    const wchar_t* without;
    const wchar_t* with;
  } kExpectations[] = {
      {L"", L"-full"},
      {L"somevaluebutnothing", L"somevaluebutnothing-full"},
      {L"full", L"full-full"},
      {L"-fullspam", L"-fullspam-full"},
  };
  for (const auto& expectation : kExpectations) {
    SCOPED_TRACE(::testing::Message()
                 << "without=\"" << expectation.without << "\" with=\""
                 << expectation.with << "\"");
    ASSERT_NO_FATAL_FAILURE(SetAp(expectation.without));
    AdditionalParameters ap;

    // Add -full.
    EXPECT_TRUE(ap.SetFullSuffix(true));
    ASSERT_TRUE(ap.Commit());
    EXPECT_EQ(GetAp(), std::optional<std::wstring>(expectation.with));

    // Remove -full.
    EXPECT_TRUE(ap.SetFullSuffix(false));
    ASSERT_TRUE(ap.Commit());
    if (!*expectation.without) {
      EXPECT_EQ(GetAp(), std::nullopt);
    } else {
      EXPECT_EQ(GetAp(), std::optional<std::wstring>(expectation.without));
    }
  }
}

TEST_F(AdditionalParametersTest, ParseChannel) {
  static constexpr struct {
    const wchar_t* ap;
    const wchar_t* expected_channel;
  } kExpectations[] = {
      // clang-format off
      {L"extended", L"extended"},
      {L"extended-arch_x86", L"extended"},
      {L"extended-arch_x64", L"extended"},
      {L"", L""},
      {L"stable-arch_x86", L""},
      {L"-arch_x86", L""},
      {L"-arch_x64", L""},
      {L"x64-stable", L""},
      {L"1.1-beta", L"beta"},
      {L"1.1-beta-arch_x86", L"beta"},
      {L"1.1-beta-statsdef_0", L"beta"},
      {L"1.1-beta-full", L"beta"},
      {L"1.1-beta-statsdef_0-full", L"beta"},
      {L"x64-stable-statsdef_0-full", L""},
      {L"noisex64-beta-statsdef_0-full", L"beta"},
      {L"noisex86-beta-statsdef_0-full", L"beta"},
      {L"noisex64-stable-statsdef_0-full", L""},
      {L"noisex86-stable-statsdef_0-full", L""},
      {L"noisex64-dev-statsdef_0-full", L"dev"},
      {L"noisex86-dev-statsdef_0-full", L"dev"},
      {L"2.0-dev", L"dev"},
      {L"2.0-dev-", L"dev"},
      // clang-format on
  };
  for (const auto& expectation : kExpectations) {
    SCOPED_TRACE(::testing::Message() << "ap=\"" << expectation.ap << "\"");
    ASSERT_NO_FATAL_FAILURE(SetAp(expectation.ap));
    AdditionalParameters ap;

    EXPECT_EQ(ap.ParseChannel(), expectation.expected_channel);
  }
}

TEST_F(AdditionalParametersTest, SetChannel) {
  static constexpr struct {
    const wchar_t* ap;
    bool has_arch;
  } kExpectations[] = {
      // clang-format off
      {L"extended", /*has_arch=*/false},
      {L"extended-arch_x86", /*has_arch=*/true},
      {L"extended-arch_x64", /*has_arch=*/true},
      {L"extended-arch_arm64", /*has_arch=*/true},
      {L"", /*has_arch=*/false},
      {L"stable-arch_x86", /*has_arch=*/true},
      {L"-arch_x86", /*has_arch=*/true},
      {L"-arch_x64", /*has_arch=*/true},
      {L"-arch_arm64", /*has_arch=*/true},
      {L"x64-stable", /*has_arch=*/true},
      {L"arm64-stable", /*has_arch=*/true},
      {L"1.1-beta", /*has_arch=*/false},
      {L"1.1-beta-arch_x86", /*has_arch=*/true},
      {L"1.1-beta-statsdef_0", /*has_arch=*/false},
      {L"1.1-beta-full", /*has_arch=*/false},
      {L"1.1-beta-statsdef_0-full", /*has_arch=*/false},
      {L"x64-stable-statsdef_0-full", /*has_arch=*/true},
      {L"noisex64-beta-statsdef_0-full", /*has_arch=*/true},
      {L"noisex86-beta-statsdef_0-full", /*has_arch=*/true},
      {L"noisex64-stable-statsdef_0-full", /*has_arch=*/true},
      {L"noisex86-stable-statsdef_0-full", /*has_arch=*/true},
      {L"noisex64-dev-statsdef_0-full", /*has_arch=*/true},
      {L"noisex86-dev-statsdef_0-full", /*has_arch=*/true},
      {L"2.0-dev", /*has_arch=*/false},
      {L"2.0-dev-", /*has_arch=*/false},
      // clang-format on
  };
  for (const auto& expectation : kExpectations) {
    SCOPED_TRACE(::testing::Message() << "ap=\"" << expectation.ap << "\"");
    ASSERT_NO_FATAL_FAILURE(SetAp(expectation.ap));

    static constexpr struct {
      version_info::Channel channel;
      bool is_extended_stable_channel;
      std::wstring_view prefix;
    } kChannels[] = {
        {version_info::Channel::DEV, /*is_extended_stable_channel=*/false,
         L"2.0-dev"},
        {version_info::Channel::BETA, /*is_extended_stable_channel=*/false,
         L"1.1-beta"},
        {version_info::Channel::STABLE, /*is_extended_stable_channel=*/false,
         L""},
        {version_info::Channel::STABLE, /*is_extended_stable_channel=*/true,
         L"extended"},
    };
    for (const auto& channel : kChannels) {
      SCOPED_TRACE(::testing::Message()
                   << "channel=" << static_cast<int>(channel.channel)
                   << " is_extended_stable_channel="
                   << (channel.is_extended_stable_channel ? "true" : "false"));
      AdditionalParameters ap;
      ap.SetChannel(channel.channel, channel.is_extended_stable_channel);
      if (channel.channel == version_info::Channel::STABLE &&
          !channel.is_extended_stable_channel) {
        if (expectation.has_arch) {
#if defined(ARCH_CPU_X86_64)
          EXPECT_THAT(ap.value(), ::testing::StartsWith(L"x64-stable"));
#elif defined(ARCH_CPU_X86)
          EXPECT_THAT(ap.value(), ::testing::StartsWith(L"stable-arch_x86"));
#elif defined(ARCH_CPU_ARM64)
          EXPECT_THAT(ap.value(), ::testing::StartsWith(L"arm64-stable"));
#else
#error unsupported processor architecture.
#endif
        } else if (*ap.value()) {
          // If there's no arch specifier, then the value should start with -.
          EXPECT_THAT(ap.value(), ::testing::StartsWith(L"-"));
        }
      } else {
        EXPECT_THAT(ap.value(),
                    ::testing::StartsWith(std::wstring(channel.prefix)));
        if (expectation.has_arch)
#if defined(ARCH_CPU_X86_64)
          EXPECT_THAT(ap.value(), ::testing::HasSubstr(L"-arch_x64"));
#elif defined(ARCH_CPU_X86)
          EXPECT_THAT(ap.value(), ::testing::HasSubstr(L"-arch_x86"));
#elif defined(ARCH_CPU_ARM64)
          EXPECT_THAT(ap.value(), ::testing::HasSubstr(L"-arch_arm64"));
#else
#error unsupported processor architecture.
#endif
      }
    }
  }
}

}  // namespace installer
