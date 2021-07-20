// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/additional_parameters.h"

#include "base/strings/string_piece.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  static absl::optional<std::wstring> GetAp() {
    std::wstring value;
    if (base::win::RegKey(HKEY_CURRENT_USER,
                          install_static::GetClientStateKeyPath().c_str(),
                          KEY_WOW64_32KEY | KEY_QUERY_VALUE)
            .ReadValue(L"ap", &value) == ERROR_SUCCESS) {
      return std::move(value);
    }
    return absl::nullopt;
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
    EXPECT_EQ(GetAp(), absl::nullopt);
  }

  {
    AdditionalParameters ap;
    EXPECT_TRUE(ap.SetFullSuffix(true));
    ASSERT_TRUE(ap.Commit());
    EXPECT_EQ(GetAp(), absl::optional<std::wstring>(L"-full"));
  }
}

TEST_F(AdditionalParametersTest, SetFullSuffixNoValue) {
  ASSERT_NO_FATAL_FAILURE(CreateKey());
  {
    AdditionalParameters ap;
    EXPECT_FALSE(ap.SetFullSuffix(false));
    EXPECT_EQ(GetAp(), absl::nullopt);
  }

  {
    AdditionalParameters ap;
    EXPECT_TRUE(ap.SetFullSuffix(true));
    ASSERT_TRUE(ap.Commit());
    EXPECT_EQ(GetAp(), absl::optional<std::wstring>(L"-full"));
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
    EXPECT_EQ(GetAp(), absl::optional<std::wstring>(expectation.with));

    // Remove -full.
    EXPECT_TRUE(ap.SetFullSuffix(false));
    ASSERT_TRUE(ap.Commit());
    if (!*expectation.without) {
      EXPECT_EQ(GetAp(), absl::nullopt);
    } else {
      EXPECT_EQ(GetAp(), absl::optional<std::wstring>(expectation.without));
    }
  }
}

}  // namespace installer
