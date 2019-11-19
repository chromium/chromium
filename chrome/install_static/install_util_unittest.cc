// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_util.h"

#include <objbase.h>

#include <tuple>

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::StrCaseEq;

namespace install_static {

// Tests the MatchPattern function in the install_static library.
TEST(InstallStaticTest, MatchPattern) {
  EXPECT_TRUE(MatchPattern(L"", L""));
  EXPECT_TRUE(MatchPattern(L"", L"*"));
  EXPECT_FALSE(MatchPattern(L"", L"*a"));
  EXPECT_FALSE(MatchPattern(L"", L"abc"));
  EXPECT_TRUE(MatchPattern(L"Hello1234", L"He??o*1*"));
  EXPECT_TRUE(MatchPattern(L"Foo", L"F*?"));
  EXPECT_TRUE(MatchPattern(L"Foo", L"F*"));
  EXPECT_FALSE(MatchPattern(L"Foo", L"F*b"));
  EXPECT_TRUE(MatchPattern(L"abcd", L"*c*d"));
  EXPECT_TRUE(MatchPattern(L"abcd", L"*?c*d"));
  EXPECT_FALSE(MatchPattern(L"abcd", L"abcd*efgh"));
  EXPECT_TRUE(MatchPattern(L"foobarabc", L"*bar*"));
}

// Tests the install_static::GetSwitchValueFromCommandLine function.
TEST(InstallStaticTest, GetSwitchValueFromCommandLineTest) {
  // Simple case with one switch.
  std::wstring value =
      GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type=bar", L"type");
  EXPECT_EQ(L"bar", value);

  // Multiple switches with trailing spaces between them.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar  --abc=def bleh", L"abc");
  EXPECT_EQ(L"def", value);

  // Multiple switches with trailing spaces and tabs between them.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar \t\t\t --abc=def bleh", L"abc");
  EXPECT_EQ(L"def", value);

  // Non existent switch.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --foo=bar  --abc=def bleh", L"type");
  EXPECT_EQ(L"", value);

  // Non existent switch.
  value = GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe", L"type");
  EXPECT_EQ(L"", value);

  // Non existent switch.
  value =
      GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe type=bar", L"type");
  EXPECT_EQ(L"", value);

  // Trailing spaces after the switch.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar      \t\t", L"type");
  EXPECT_EQ(L"bar", value);

  // Multiple switches with trailing spaces and tabs between them.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar      \t\t --foo=bleh", L"foo");
  EXPECT_EQ(L"bleh", value);

  // Nothing after a switch.
  value = GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type=", L"type");
  EXPECT_TRUE(value.empty());

  // Whitespace after a switch.
  value =
      GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type= ", L"type");
  EXPECT_TRUE(value.empty());

  // Just tabs after a switch.
  value = GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type=\t\t\t",
                                        L"type");
  EXPECT_TRUE(value.empty());

  // Bad command line without closing quotes. Should not crash.
  value = GetSwitchValueFromCommandLine(L"\"blah --type=\t\t\t", L"type");
  EXPECT_TRUE(value.empty());

  // Anything following "--" should be considered args and therfore ignored.
  value = GetSwitchValueFromCommandLine(L"blah -- --type=bleh", L"type");
  EXPECT_TRUE(value.empty());
}

TEST(InstallStaticTest, SpacesAndQuotesInCommandLineArguments) {
  std::vector<std::wstring> tokenized;

  tokenized = TokenizeCommandLineToArray(L"\"C:\\a\\b.exe\"");
  ASSERT_EQ(1u, tokenized.size());
  EXPECT_EQ(L"C:\\a\\b.exe", tokenized[0]);

  tokenized = TokenizeCommandLineToArray(L"x.exe");
  ASSERT_EQ(1u, tokenized.size());
  EXPECT_EQ(L"x.exe", tokenized[0]);

  tokenized = TokenizeCommandLineToArray(L"\"c:\\with space\\something.exe\"");
  ASSERT_EQ(1u, tokenized.size());
  EXPECT_EQ(L"c:\\with space\\something.exe", tokenized[0]);

  tokenized = TokenizeCommandLineToArray(L"\"C:\\a\\b.exe\" arg");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\a\\b.exe", tokenized[0]);
  EXPECT_EQ(L"arg", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(L"\"C:\\with space\\b.exe\" \"arg\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"arg", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(L"\"C:\\a\\b.exe\" c:\\tmp\\");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\a\\b.exe", tokenized[0]);
  EXPECT_EQ(L"c:\\tmp\\", tokenized[1]);

  tokenized =
      TokenizeCommandLineToArray(L"\"C:\\a\\b.exe\" \"c:\\some file path\\\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\a\\b.exe", tokenized[0]);
  EXPECT_EQ(L"c:\\some file path\"", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(
      L"\"C:\\with space\\b.exe\" \\\\x\\\\ \\\\y\\\\");
  ASSERT_EQ(3u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"\\\\x\\\\", tokenized[1]);
  EXPECT_EQ(L"\\\\y\\\\", tokenized[2]);

  tokenized = TokenizeCommandLineToArray(
      L"\"C:\\with space\\b.exe\" \"\\\\space quoted\\\\\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"\\\\space quoted\\", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(
      L"\"C:\\with space\\b.exe\" --stuff    -x -Y   \"c:\\some thing\\\"    "
      L"weewaa    ");
  ASSERT_EQ(5u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"--stuff", tokenized[1]);
  EXPECT_EQ(L"-x", tokenized[2]);
  EXPECT_EQ(L"-Y", tokenized[3]);
  EXPECT_EQ(L"c:\\some thing\"    weewaa    ", tokenized[4]);

  tokenized = TokenizeCommandLineToArray(
      L"\"C:\\with space\\b.exe\" --stuff=\"d:\\stuff and things\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"--stuff=d:\\stuff and things", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(
      L"\"C:\\with space\\b.exe\" \\\\\\\"\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"\\\"", tokenized[1]);

  tokenized =
      TokenizeCommandLineToArray(L"\"blah --type=\t\t\t no closing quote");
  ASSERT_EQ(1u, tokenized.size());
  EXPECT_EQ(L"blah --type=\t\t\t no closing quote", tokenized[0]);
}

// Test cases from
// https://blogs.msdn.microsoft.com/oldnewthing/20100917-00/?p=12833.
TEST(InstallStaticTest, SpacesAndQuotesOldNewThing) {
  std::vector<std::wstring> tokenized;

  tokenized = TokenizeCommandLineToArray(L"program.exe \"hello there.txt\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"program.exe", tokenized[0]);
  EXPECT_EQ(L"hello there.txt", tokenized[1]);

  tokenized =
      TokenizeCommandLineToArray(L"program.exe \"C:\\Hello there.txt\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"program.exe", tokenized[0]);
  EXPECT_EQ(L"C:\\Hello there.txt", tokenized[1]);

  tokenized =
      TokenizeCommandLineToArray(L"program.exe \"hello\\\"there\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"program.exe", tokenized[0]);
  EXPECT_EQ(L"hello\"there", tokenized[1]);

  tokenized =
      TokenizeCommandLineToArray(L"program.exe \"hello\\\\\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"program.exe", tokenized[0]);
  EXPECT_EQ(L"hello\\", tokenized[1]);
}

// Test cases from
// http://www.windowsinspired.com/how-a-windows-programs-splits-its-command-line-into-individual-arguments/.
// These are mostly about the special handling of argv[0], which uses different
// quoting than the rest of the arguments.
TEST(InstallStaticTest, SpacesAndQuotesWindowsInspired) {
  std::vector<std::wstring> tokenized;

  tokenized = TokenizeCommandLineToArray(
      L"\"They said \"you can't do this!\", didn't they?\"");
  ASSERT_EQ(5u, tokenized.size());
  EXPECT_EQ(L"They said ", tokenized[0]);
  EXPECT_EQ(L"you", tokenized[1]);
  EXPECT_EQ(L"can't", tokenized[2]);
  EXPECT_EQ(L"do", tokenized[3]);
  EXPECT_EQ(L"this!, didn't they?", tokenized[4]);

  tokenized = TokenizeCommandLineToArray(
      L"test.exe \"c:\\Path With Spaces\\Ending In Backslash\\\" Arg2 Arg3");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"test.exe", tokenized[0]);
  EXPECT_EQ(L"c:\\Path With Spaces\\Ending In Backslash\" Arg2 Arg3",
            tokenized[1]);

  tokenized = TokenizeCommandLineToArray(
      L"FinalProgram.exe \"first second \"\"embedded quote\"\" third\"");
  ASSERT_EQ(4u, tokenized.size());
  EXPECT_EQ(L"FinalProgram.exe", tokenized[0]);
  EXPECT_EQ(L"first second \"embedded", tokenized[1]);
  EXPECT_EQ(L"quote", tokenized[2]);
  EXPECT_EQ(L"third", tokenized[3]);

  tokenized = TokenizeCommandLineToArray(
      L"\"F\"i\"r\"s\"t S\"e\"c\"o\"n\"d\" T\"h\"i\"r\"d\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"F", tokenized[0]);
  EXPECT_EQ(L"irst Second Third", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(L"F\"\"ir\"s\"\"t \\\"Second Third\"");
  ASSERT_EQ(3u, tokenized.size());
  EXPECT_EQ(L"F\"\"ir\"s\"\"t", tokenized[0]);
  EXPECT_EQ(L"\"Second", tokenized[1]);
  EXPECT_EQ(L"Third", tokenized[2]);

  tokenized = TokenizeCommandLineToArray(L"  Something Else");
  ASSERT_EQ(3u, tokenized.size());
  EXPECT_EQ(L"", tokenized[0]);
  EXPECT_EQ(L"Something", tokenized[1]);
  EXPECT_EQ(L"Else", tokenized[2]);

  tokenized = TokenizeCommandLineToArray(L" Something Else");
  ASSERT_EQ(3u, tokenized.size());
  EXPECT_EQ(L"", tokenized[0]);
  EXPECT_EQ(L"Something", tokenized[1]);
  EXPECT_EQ(L"Else", tokenized[2]);

  tokenized = TokenizeCommandLineToArray(L"\"123 456\tabc\\def\"ghi");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"123 456\tabc\\def", tokenized[0]);
  EXPECT_EQ(L"ghi", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(L"123\"456\"\tabc");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"123\"456\"", tokenized[0]);
  EXPECT_EQ(L"abc", tokenized[1]);
}

TEST(InstallStaticTest, BrowserProcessTest) {
  EXPECT_FALSE(IsProcessTypeInitialized());
  InitializeProcessType();
  EXPECT_FALSE(IsNonBrowserProcess());
}

class InstallStaticUtilTest
    : public ::testing::TestWithParam<
          std::tuple<InstallConstantIndex, const char*>> {
 protected:
  InstallStaticUtilTest()
      : system_level_(std::string(std::get<1>(GetParam())) != "user"),
        scoped_install_details_(system_level_, std::get<0>(GetParam())),
        mode_(&InstallDetails::Get().mode()),
        root_key_(system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER),
        nt_root_key_(system_level_ ? nt::HKLM : nt::HKCU) {}

  void SetUp() override {
    ASSERT_TRUE(!system_level_ || mode_->supports_system_level);
    base::string16 path;
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(root_key_, &path));
    nt::SetTestingOverride(nt_root_key_, path);
  }

  void TearDown() override {
    nt::SetTestingOverride(nt_root_key_, base::string16());
  }

  bool system_level() const { return system_level_; }

  const wchar_t* default_channel() const { return mode_->default_channel_name; }

  void SetUsageStat(DWORD value, bool medium) {
    ASSERT_TRUE(!medium || system_level_);
    ASSERT_EQ(ERROR_SUCCESS,
              base::win::RegKey(root_key_, GetUsageStatsKeyPath(medium).c_str(),
                                KEY_SET_VALUE | KEY_WOW64_32KEY)
                  .WriteValue(L"usagestats", value));
  }

  void SetMetricsReportingPolicy(DWORD value) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    static constexpr wchar_t kPolicyKey[] =
        L"Software\\Policies\\Google\\Chrome";
#else
    static constexpr wchar_t kPolicyKey[] = L"Software\\Policies\\Chromium";
#endif

    ASSERT_EQ(ERROR_SUCCESS,
              base::win::RegKey(root_key_, kPolicyKey, KEY_SET_VALUE)
                  .WriteValue(L"MetricsReportingEnabled", value));
  }

 private:
  // Returns the registry path for the key holding the product's usagestats
  // value. |medium| = true returns the path for ClientStateMedium.
  std::wstring GetUsageStatsKeyPath(bool medium) {
    EXPECT_TRUE(!medium || system_level_);

    std::wstring result(L"Software\\");
    if (kUseGoogleUpdateIntegration) {
      result.append(L"Google\\Update\\ClientState");
      if (medium)
        result.append(L"Medium");
      result.push_back(L'\\');
      result.append(mode_->app_guid);
    } else {
      result.append(kProductPathName);
    }
    return result;
  }

  const bool system_level_;
  const ScopedInstallDetails scoped_install_details_;
  const InstallConstants* mode_;
  const HKEY root_key_;
  const nt::ROOT_KEY nt_root_key_;
  registry_util::RegistryOverrideManager override_manager_;

  DISALLOW_COPY_AND_ASSIGN(InstallStaticUtilTest);
};

TEST_P(InstallStaticUtilTest, GetChromeInstallSubDirectory) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The directory strings for the brand's install modes; parallel to
  // kInstallModes.
  static constexpr const wchar_t* kInstallDirs[] = {
      L"Google\\Chrome", L"Google\\Chrome Beta", L"Google\\Chrome Dev",
      L"Google\\Chrome SxS",
  };
#else
  // The directory strings for the brand's install modes; parallel to
  // kInstallModes.
  static constexpr const wchar_t* kInstallDirs[] = {
      L"Chromium",
  };
#endif
  static_assert(base::size(kInstallDirs) == NUM_INSTALL_MODES,
                "kInstallDirs out of date.");
  EXPECT_THAT(GetChromeInstallSubDirectory(),
              StrCaseEq(kInstallDirs[std::get<0>(GetParam())]));
}

TEST_P(InstallStaticUtilTest, GetRegistryPath) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The registry path strings for the brand's install modes; parallel to
  // kInstallModes.
  static constexpr const wchar_t* kRegistryPaths[] = {
      L"Software\\Google\\Chrome", L"Software\\Google\\Chrome Beta",
      L"Software\\Google\\Chrome Dev", L"Software\\Google\\Chrome SxS",
  };
#else
  // The registry path strings for the brand's install modes; parallel to
  // kInstallModes.
  static constexpr const wchar_t* kRegistryPaths[] = {
      L"Software\\Chromium",
  };
#endif
  static_assert(base::size(kRegistryPaths) == NUM_INSTALL_MODES,
                "kRegistryPaths out of date.");
  EXPECT_THAT(GetRegistryPath(),
              StrCaseEq(kRegistryPaths[std::get<0>(GetParam())]));
}

TEST_P(InstallStaticUtilTest, GetUninstallRegistryPath) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The uninstall registry path strings for the brand's install modes; parallel
  // to kInstallModes.
  static constexpr const wchar_t* kUninstallRegistryPaths[] = {
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Google Chrome",
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"  // (cont'd)
      L"Google Chrome Beta",
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"  // (cont'd)
      L"Google Chrome Dev",
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"  // (cont'd)
      L"Google Chrome SxS",
  };
#else
  // The registry path strings for the brand's install modes; parallel to
  // kInstallModes.
  static constexpr const wchar_t* kUninstallRegistryPaths[] = {
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Chromium",
  };
#endif
  static_assert(base::size(kUninstallRegistryPaths) == NUM_INSTALL_MODES,
                "kUninstallRegistryPaths out of date.");
  EXPECT_THAT(GetUninstallRegistryPath(),
              StrCaseEq(kUninstallRegistryPaths[std::get<0>(GetParam())]));
}

TEST_P(InstallStaticUtilTest, GetAppGuid) {
  // For brands that do not integrate with Omaha/Google Update, the app guid is
  // an empty string.
  if (!kUseGoogleUpdateIntegration) {
    EXPECT_STREQ(L"", GetAppGuid());
    return;
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The app guids for the brand's install modes; parallel to kInstallModes.
  static constexpr const wchar_t* kAppGuids[] = {
      L"{8A69D345-D564-463c-AFF1-A69D9E530F96}",  // Google Chrome.
      L"{8237E44A-0054-442C-B6B6-EA0509993955}",  // Google Chrome Beta.
      L"{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}",  // Google Chrome Dev.
      L"{4EA16AC7-FD5A-47C3-875B-DBF4A2008C20}",  // Google Chrome SxS (Canary).
  };
  static_assert(base::size(kAppGuids) == NUM_INSTALL_MODES,
                "kAppGuids out of date.");
  EXPECT_THAT(GetAppGuid(), StrCaseEq(kAppGuids[std::get<0>(GetParam())]));
#else
  FAIL() << "Not implemented.";
#endif
}

TEST_P(InstallStaticUtilTest, GetBaseAppId) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The base app ids for the brand's install modes; parallel to kInstallModes.
  static constexpr const wchar_t* kBaseAppIds[] = {
      L"Chrome", L"ChromeBeta", L"ChromeDev", L"ChromeCanary",
  };
#else
  // The base app ids for the brand's install modes; parallel to kInstallModes.
  static constexpr const wchar_t* kBaseAppIds[] = {
      L"Chromium",
  };
#endif
  static_assert(base::size(kBaseAppIds) == NUM_INSTALL_MODES,
                "kBaseAppIds out of date.");
  EXPECT_THAT(GetBaseAppId(), StrCaseEq(kBaseAppIds[std::get<0>(GetParam())]));
}

TEST_P(InstallStaticUtilTest, GetToastActivatorClsid) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The toast activator CLSIDs for the brand's install modes; parallel to
  // kInstallModes.
  static constexpr CLSID kToastActivatorClsids[] = {
      {0xA2C6CB58,
       0xC076,
       0x425C,
       {0xAC, 0xB7, 0x6D, 0x19, 0xD6, 0x44, 0x28, 0xCD}},  // Google Chrome.
      {0xB89B137F,
       0x96AA,
       0x4AE2,
       {0x98, 0xC4, 0x63, 0x73, 0xEA, 0xA1, 0xEA,
        0x4D}},  // Google Chrome Beta.
      {0xF01C03EB,
       0xD431,
       0x4C83,
       {0x8D, 0x7A, 0x90, 0x27, 0x71, 0xE7, 0x32, 0xFA}},  // Google Chrome Dev.
      {0xFA372A6E,
       0x149F,
       0x4E95,
       {0x83, 0x2D, 0x8F, 0x69, 0x8D, 0x40, 0xAD,
        0x7F}}  // Google Chrome SxS (Canary).
  };

  // The string representation of the CLSIDs above.
  static constexpr const wchar_t* kToastActivatorClsidsString[] = {
      L"{A2C6CB58-C076-425C-ACB7-6D19D64428CD}",  // Google Chrome.
      L"{B89B137F-96AA-4AE2-98C4-6373EAA1EA4D}",  // Google Chrome Beta.
      L"{F01C03EB-D431-4C83-8D7A-902771E732FA}",  // Google Chrome Dev.
      L"{FA372A6E-149F-4E95-832D-8F698D40AD7F}",  // Google Chrome SxS (Canary).
  };
#else
  // The toast activator CLSIDs for the brand's install modes; parallel to
  // kInstallModes.
  static constexpr CLSID kToastActivatorClsids[] = {
      {0x635EFA6F,
       0x08D6,
       0x4EC9,
       {0xBD, 0x14, 0x8A, 0x0F, 0xDE, 0x97, 0x51, 0x59}}  // Chromium.
  };

  // The string representation of the CLSIDs above.
  static constexpr const wchar_t* kToastActivatorClsidsString[] = {
      L"{635EFA6F-08D6-4EC9-BD14-8A0FDE975159}"  // Chromium.
  };
#endif
  static_assert(base::size(kToastActivatorClsids) == NUM_INSTALL_MODES,
                "kToastActivatorClsids out of date.");

  EXPECT_EQ(GetToastActivatorClsid(),
            kToastActivatorClsids[std::get<0>(GetParam())]);

  auto clsid_str = base::win::String16FromGUID(GetToastActivatorClsid());
  EXPECT_THAT(base::as_wcstr(clsid_str.c_str()),
              StrCaseEq(kToastActivatorClsidsString[std::get<0>(GetParam())]));
}

TEST_P(InstallStaticUtilTest, GetElevatorClsid) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The Elevator CLSIDs, one for each of the kInstallModes.
  static constexpr CLSID kElevatorClsids[] = {
      {0x708860E0,
       0xF641,
       0x4611,
       {0x88, 0x95, 0x7D, 0x86, 0x7D, 0xD3, 0x67, 0x5B}},  // Google Chrome.
      {0xDD2646BA,
       0x3707,
       0x4BF8,
       {0xB9, 0xA7, 0x3, 0x86, 0x91, 0xA6, 0x8F, 0xC2}},  // Google Chrome Beta.
      {0xDA7FDCA5,
       0x2CAA,
       0x4637,
       {0xAA, 0x17, 0x7, 0x40, 0x58, 0x4D, 0xE7, 0xDA}},  // Google Chrome Dev.
      {0x704C2872,
       0x2049,
       0x435E,
       {0xA4, 0x69, 0xA, 0x53, 0x43, 0x13, 0xC4,
        0x2B}},  // Google Chrome SxS (Canary).
  };

  // The string representation of the CLSIDs above.
  static constexpr const wchar_t* kElevatorClsidsString[] = {
      L"{708860E0-F641-4611-8895-7D867DD3675B}",  // Google Chrome.
      L"{DD2646BA-3707-4BF8-B9A7-038691A68FC2}",  // Google Chrome Beta.
      L"{DA7FDCA5-2CAA-4637-AA17-0740584DE7DA}",  // Google Chrome Dev.
      L"{704C2872-2049-435E-A469-0A534313C42B}",  // Google Chrome SxS (Canary).
  };
#else
  // The Elevator CLSIDs, one for each of the kInstallModes.
  static constexpr CLSID kElevatorClsids[] = {
      {0xD133B120,
       0x6DB4,
       0x4D6B,
       {0x8B, 0xFE, 0x83, 0xBF, 0x8C, 0xA1, 0xB1, 0xB0}},  // Chromium.
  };

  // The string representation of the CLSIDs above.
  static constexpr const wchar_t* kElevatorClsidsString[] = {
      L"{D133B120-6DB4-4D6B-8BFE-83BF8CA1B1B0}",  // Chromium.
  };
#endif
  static_assert(base::size(kElevatorClsids) == NUM_INSTALL_MODES,
                "kElevatorClsids needs to be updated for any new modes.");

  EXPECT_EQ(GetElevatorClsid(), kElevatorClsids[std::get<0>(GetParam())]);

  auto clsid_str = base::win::String16FromGUID(GetElevatorClsid());
  EXPECT_THAT(base::as_wcstr(clsid_str.c_str()),
              StrCaseEq(kElevatorClsidsString[std::get<0>(GetParam())]));
}

TEST_P(InstallStaticUtilTest, GetElevatorIid) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The Elevator IIDs, one for each of the kInstallModes.
  static constexpr IID kElevatorIids[] = {
      {0x463abecf,
       0x410d,
       0x407f,
       {0x8a, 0xf5, 0xd, 0xf3, 0x5a, 0x0, 0x5c,
        0xc8}},  // IElevator IID and TypeLib
                 // {463ABECF-410D-407F-8AF5-0DF35A005CC8} for Google Chrome.
      {0xa2721d66,
       0x376e,
       0x4d2f,
       {0x9f, 0xf, 0x90, 0x70, 0xe9, 0xa4, 0x2b,
        0x5f}},  // IElevator IID and TypeLib
                 // {A2721D66-376E-4D2F-9F0F-9070E9A42B5F} for Google Chrome
                 // Beta.
      {0xbb2aa26b,
       0x343a,
       0x4072,
       {0x8b, 0x6f, 0x80, 0x55, 0x7b, 0x8c, 0xe5,
        0x71}},  // IElevator IID and TypeLib
                 // {BB2AA26B-343A-4072-8B6F-80557B8CE571} for Google Chrome
                 // Dev.
      {0x4f7ce041,
       0x28e9,
       0x484f,
       {0x9d, 0xd0, 0x61, 0xa8, 0xca, 0xce, 0xfe,
        0xe4}},  // IElevator IID and TypeLib
                 // {4F7CE041-28E9-484F-9DD0-61A8CACEFEE4} for Google Chrome
                 // Canary.
  };

  // The string representation of the IIDs above.
  static constexpr const wchar_t* kElevatorIidsString[] = {
      L"{463ABECF-410D-407F-8AF5-0DF35A005CC8}",  // Google Chrome.
      L"{A2721D66-376E-4D2F-9F0F-9070E9A42B5F}",  // Google Chrome Beta.
      L"{BB2AA26B-343A-4072-8B6F-80557B8CE571}",  // Google Chrome Dev.
      L"{4F7CE041-28E9-484F-9DD0-61A8CACEFEE4}",  // Google Chrome Canary.
  };
#else
  // The Elevator IIDs, one for each of the kInstallModes.
  static constexpr IID kElevatorIids[] = {
      {0xb88c45b9,
       0x8825,
       0x4629,
       {0xb8, 0x3e, 0x77, 0xcc, 0x67, 0xd9, 0xce,
        0xed}},  // IElevator IID and TypeLib
                 // {B88C45B9-8825-4629-B83E-77CC67D9CEED} for Chromium.
  };

  // The string representation of the IIDs above.
  static constexpr const wchar_t* kElevatorIidsString[] = {
      L"{B88C45B9-8825-4629-B83E-77CC67D9CEED}",  // Chromium.
  };
#endif
  static_assert(base::size(kElevatorIids) == NUM_INSTALL_MODES,
                "kElevatorIids needs to be updated for any new modes.");

  EXPECT_EQ(GetElevatorIid(), kElevatorIids[std::get<0>(GetParam())]);

  auto iid_str = base::win::String16FromGUID(GetElevatorIid());
  EXPECT_THAT(base::as_wcstr(iid_str.c_str()),
              StrCaseEq(kElevatorIidsString[std::get<0>(GetParam())]));
}

TEST_P(InstallStaticUtilTest, UsageStatsAbsent) {
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsZero) {
  SetUsageStat(0, false);
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsZeroMedium) {
  if (!system_level())
    return;
  SetUsageStat(0, true);
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsOne) {
  SetUsageStat(1, false);
  EXPECT_TRUE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsOneMedium) {
  if (!system_level())
    return;
  SetUsageStat(1, true);
  EXPECT_TRUE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, ReportingIsEnforcedByPolicy) {
  bool reporting_enabled = false;
  EXPECT_FALSE(ReportingIsEnforcedByPolicy(&reporting_enabled));

  SetMetricsReportingPolicy(0);
  EXPECT_TRUE(ReportingIsEnforcedByPolicy(&reporting_enabled));
  EXPECT_FALSE(reporting_enabled);

  SetMetricsReportingPolicy(1);
  EXPECT_TRUE(ReportingIsEnforcedByPolicy(&reporting_enabled));
  EXPECT_TRUE(reporting_enabled);
}

TEST_P(InstallStaticUtilTest, UsageStatsPolicy) {
  // Policy alone.
  SetMetricsReportingPolicy(0);
  EXPECT_FALSE(GetCollectStatsConsent());

  SetMetricsReportingPolicy(1);
  EXPECT_TRUE(GetCollectStatsConsent());

  // Policy trumps usagestats.
  SetMetricsReportingPolicy(1);
  SetUsageStat(0, false);
  EXPECT_TRUE(GetCollectStatsConsent());

  SetMetricsReportingPolicy(0);
  SetUsageStat(1, false);
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, GetChromeChannelName) {
  EXPECT_EQ(default_channel(), GetChromeChannelName());
}

TEST_P(InstallStaticUtilTest, GetSandboxSidPrefix) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr const wchar_t* kSandBoxSids[] = {
      L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
      L"924012149-",  // Google Chrome.
      L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
      L"924012151-",  // Google Chrome Beta.
      L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
      L"924012152-",  // Google Chrome Dev.
      L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
      L"924012150-",  // Google Chrome SxS (Canary).
  };
#else
  static constexpr const wchar_t* kSandBoxSids[] = {
      L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
      L"924012148-",  // Chromium.
  };
#endif
  EXPECT_STREQ(GetSandboxSidPrefix(), kSandBoxSids[std::get<0>(GetParam())]);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Stable supports user and system levels.
INSTANTIATE_TEST_SUITE_P(Stable,
                         InstallStaticUtilTest,
                         testing::Combine(testing::Values(STABLE_INDEX),
                                          testing::Values("user", "system")));
// Beta supports user and system levels.
INSTANTIATE_TEST_SUITE_P(Beta,
                         InstallStaticUtilTest,
                         testing::Combine(testing::Values(BETA_INDEX),
                                          testing::Values("user", "system")));
// Dev supports user and system levels.
INSTANTIATE_TEST_SUITE_P(Dev,
                         InstallStaticUtilTest,
                         testing::Combine(testing::Values(DEV_INDEX),
                                          testing::Values("user", "system")));
// Canary is only at user level.
INSTANTIATE_TEST_SUITE_P(Canary,
                         InstallStaticUtilTest,
                         testing::Combine(testing::Values(CANARY_INDEX),
                                          testing::Values("user")));
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Chromium supports user and system levels.
INSTANTIATE_TEST_SUITE_P(Chromium,
                         InstallStaticUtilTest,
                         testing::Combine(testing::Values(CHROMIUM_INDEX),
                                          testing::Values("user", "system")));
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace install_static
