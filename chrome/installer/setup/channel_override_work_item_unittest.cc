// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/channel_override_work_item.h"

#include <memory>
#include <optional>
#include <string>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "build/build_config.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChannelOverrideWorkItemTest : public ::testing::Test {
 protected:
  struct TestParam {
    const wchar_t* input_ap;
    const wchar_t* expected;
  };

  ChannelOverrideWorkItemTest() = default;

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

  static const wchar_t* input(const TestParam& param) { return param.input_ap; }
  static std::optional<std::wstring> optional_input(const TestParam& param) {
    auto* const input = param.input_ap;
    return input ? std::optional<std::wstring>(input) : std::nullopt;
  }
  static std::optional<std::wstring> expected(const TestParam& param) {
    auto* const expected = param.expected;
    return expected ? std::optional<std::wstring>(expected) : std::nullopt;
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
};

TEST_F(ChannelOverrideWorkItemTest, DoAndRollback) {
  static constexpr TestParam kIterations[] = {
      {L"", L""},
      {L"1.1-beta", L""},
      {L"2.0-dev", L""},
      {L"extended", L""},
#if defined(ARCH_CPU_X86_64)
      {L"x64-stable", L"x64-stable"},
      {L"1.1-beta-arch_x64", L"x64-stable"},
      {L"2.0-dev-arch_x64", L"x64-stable"},
      {L"extended-arch_x64", L"x64-stable"},
#elif defined(ARCH_CPU_X86)
      {L"stable-arch_x86", L"stable-arch_x86"},
      {L"1.1-beta-arch_x86", L"stable-arch_x86"},
      {L"2.0-dev-arch_x86", L"stable-arch_x86"},
      {L"extended-arch_x86", L"stable-arch_x86"},
#elif defined(ARCH_CPU_ARM64)
      {L"arm64-stable", L"arm64-stable"},
      {L"1.1-beta-arch_arm64", L"arm64-stable"},
      {L"2.0-dev-arch_arm64", L"arm64-stable"},
      {L"extended-arch_arm64", L"arm64-stable"},
#else
#error unsupported processor architecture.
#endif
  };
  for (const auto& iteration : kIterations) {
    SCOPED_TRACE(::testing::Message() << "input=\"" << iteration.input_ap);

    SetAp(input(iteration));

    ChannelOverrideWorkItem item;

    ASSERT_TRUE(item.Do());
    EXPECT_EQ(GetAp(), expected(iteration));
    item.Rollback();
    EXPECT_EQ(GetAp(), optional_input(iteration));
  }
}
