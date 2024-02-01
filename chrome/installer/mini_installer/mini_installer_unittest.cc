// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/mini_installer.h"

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_details.h"
#include "chrome/installer/mini_installer/configuration.h"
#include "chrome/installer/mini_installer/path_string.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mini_installer {

namespace {

#define PREVIOUS_VERSION L"62.0.1234.0"
constexpr wchar_t kPreviousVersion[] = PREVIOUS_VERSION;

class FakeConfiguration : public Configuration {
 public:
  FakeConfiguration() { previous_version_ = kPreviousVersion; }
};

}  // namespace

TEST(MiniInstallerTest, AppendCommandLineFlags) {
  static constexpr struct {
    const wchar_t* command_line;
    const wchar_t* args;
  } kData[] = {
      {L"", L"foo.exe"},
      {L"mini_installer.exe", L"foo.exe"},
      {L"mini_installer.exe --verbose-logging", L"foo.exe --verbose-logging"},
      {L"C:\\Temp\\mini_installer.exe --verbose-logging",
       L"foo.exe --verbose-logging"},
      {L"C:\\Temp\\mini_installer --verbose-logging",
       L"foo.exe --verbose-logging"},
      {L"\"C:\\Temp\\mini_installer (1).exe\" --verbose-logging",
       L"foo.exe --verbose-logging"},
      {L"\"mini_installer.exe\"--verbose-logging",
       L"foo.exe --verbose-logging"},
  };

  CommandString buffer;

  for (const auto& data : kData) {
    buffer.assign(L"foo.exe");
    AppendCommandLineFlags(data.command_line, &buffer);
    EXPECT_STREQ(data.args, buffer.get()) << data.command_line;
  }
}

TEST(MiniInstallerTest, GetModuleDir) {
  PathString directory;

  ASSERT_TRUE(GetModuleDir(/*module=*/nullptr, &directory));
  ASSERT_NE(directory.length(), 0U);
  EXPECT_LT(directory.length(), directory.capacity());
  EXPECT_EQ(directory.get()[directory.length() - 1], L'\\');
}

// A test harness for GetPreviousSetupExePath.
class GetPreviousSetupExePathTest : public ::testing::Test {
 public:
  GetPreviousSetupExePathTest(const GetPreviousSetupExePathTest&) = delete;
  GetPreviousSetupExePathTest& operator=(const GetPreviousSetupExePathTest&) =
      delete;

 protected:
  GetPreviousSetupExePathTest() = default;
  ~GetPreviousSetupExePathTest() override = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  const Configuration& configuration() const { return configuration_; }

  // Writes |path| to the registry in Chrome's ClientState...UninstallString
  // value.
  void SetPreviousSetup(const wchar_t* path) {
    base::win::RegKey key;
    const install_static::InstallDetails& details =
        install_static::InstallDetails::Get();
    ASSERT_EQ(
        key.Create(HKEY_CURRENT_USER, details.GetClientStateKeyPath().c_str(),
                   KEY_SET_VALUE | KEY_WOW64_32KEY),
        ERROR_SUCCESS);
    ASSERT_EQ(key.WriteValue(installer::kUninstallStringField, path),
              ERROR_SUCCESS);
  }

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;
  FakeConfiguration configuration_;
};

// Tests that the path is returned.
TEST_F(GetPreviousSetupExePathTest, SimpleTest) {
  static constexpr wchar_t kSetupExePath[] =
      L"C:\\SomePath\\To\\" PREVIOUS_VERSION L"\\setup.exe";
  ASSERT_NO_FATAL_FAILURE(SetPreviousSetup(kSetupExePath));

  StackString<MAX_PATH> path;
  ProcessExitResult result =
      GetPreviousSetupExePath(configuration(), path.get(), path.capacity());
  ASSERT_TRUE(result.IsSuccess());
  EXPECT_STREQ(path.get(), kSetupExePath);
}

// Tests that quotes are removed, if present.
TEST_F(GetPreviousSetupExePathTest, QuoteStripping) {
  static constexpr wchar_t kSetupExePath[] =
      L"C:\\SomePath\\To\\" PREVIOUS_VERSION L"\\setup.exe";
  std::wstring quoted_path(L"\"");
  quoted_path += kSetupExePath;
  quoted_path += L"\"";
  ASSERT_NO_FATAL_FAILURE(SetPreviousSetup(quoted_path.c_str()));

  StackString<MAX_PATH> path;
  ProcessExitResult result =
      GetPreviousSetupExePath(configuration(), path.get(), path.capacity());
  ASSERT_TRUE(result.IsSuccess());
  EXPECT_STREQ(path.get(), kSetupExePath);
}

}  // namespace mini_installer
