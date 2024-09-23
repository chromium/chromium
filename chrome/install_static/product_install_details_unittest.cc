// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/install_static/product_install_details.h"

#include <string_view>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/path_service.h"
#include "base/strings/strcat_win.h"
#include "base/strings/string_util.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/buildflags.h"
#include "chrome/install_static/install_constants.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::StrEq;

namespace install_static {

namespace {

TEST(ProductInstallDetailsTest, IsPathParentOf) {
  std::wstring path = L"C:\\Program Files\\Company\\Product\\Application\\foo";
  static constexpr const wchar_t* kFalseExpectations[] = {
      L"", L"\\", L"\\\\", L"C:\\Program File", L"C:\\Program Filesz",
  };
  for (const wchar_t* false_expectation : kFalseExpectations) {
    EXPECT_FALSE(IsPathParentOf(
        false_expectation, std::wstring::traits_type::length(false_expectation),
        path));
  }

  static constexpr const wchar_t* kTrueExpectations[] = {
      L"C:\\Program Files",
      L"C:\\PROGRAM FILES",
      L"C:\\Program Files\\",
      L"C:\\Program Files\\\\\\",
  };
  for (const wchar_t* true_expectation : kTrueExpectations) {
    EXPECT_TRUE(IsPathParentOf(
        true_expectation, std::wstring::traits_type::length(true_expectation),
        path));
  }
}

TEST(ProductInstallDetailsTest, PathIsInProgramFiles) {
  static constexpr const wchar_t* kInvalidPaths[] = {
      L"",
      L"hello",
      L"C:\\Program File",
      L"C:\\Program Filesz",
      L"C:\\foo\\Program Files",
      L"C:\\foo\\Program Files\\",
      L"C:\\foo\\Program Files (x86)",
      L"C:\\foo\\Program Files (x86)\\",
  };
  for (const wchar_t* invalid : kInvalidPaths)
    EXPECT_FALSE(PathIsInProgramFiles(invalid)) << invalid;

  // 32-bit on 32-bit: only check C:\Program Files.
  // 32-bit and 64-bit on 64-bit: check both.
  const bool is_x64 = base::win::OSInfo::GetArchitecture() !=
                      base::win::OSInfo::X86_ARCHITECTURE;
  std::vector<int> program_files_keys;
  program_files_keys.push_back(base::DIR_PROGRAM_FILESX86);
  if (is_x64)
    program_files_keys.push_back(base::DIR_PROGRAM_FILES6432);
  std::vector<std::wstring> program_files_paths;
  for (int key : program_files_keys) {
    base::FilePath path;
    ASSERT_TRUE(base::PathService::Get(key, &path));
    program_files_paths.push_back(path.value());
  }

  static constexpr const wchar_t* kValidSuffixes[] = {
      L"",
      L"\\",
      L"\\spam",
  };
  for (const wchar_t* valid : kValidSuffixes) {
    for (const std::wstring& program_files_path : program_files_paths) {
      std::wstring path = program_files_path + valid;
      EXPECT_TRUE(PathIsInProgramFiles(path)) << path;

      path = base::AsWString(base::i18n::ToLower(
                 base::AsStringPiece16(program_files_path))) +
             valid;
      EXPECT_TRUE(PathIsInProgramFiles(path)) << path;
    }
  }
}

TEST(ProductInstallDetailsTest, GetInstallSuffix) {
  std::wstring suffix;
  struct TestData {
    std::wstring_view path_prefix;
    std::wstring_view path_suffix;
    std::wstring_view install_suffix;
  };
  constexpr TestData kData[] = {
      {L"", L"\\Application", L""},
      {L"", L"\\Application\\", L""},
      {L"\\", L"\\Application", L""},
      {L"\\", L"\\Application\\", L""},
      {L"C:\\foo\\", L"\\Application\\foo.exe", L""},
      {L"", L" Blorf\\Application", L" Blorf"},
      {L"", L" Blorf\\Application\\", L" Blorf"},
      {L"\\", L" Blorf\\Application", L" Blorf"},
      {L"\\", L" Blorf\\Application\\", L" Blorf"},
      {L"C:\\foo\\", L" Blorf\\Application\\foo.exe", L" Blorf"},
  };
  for (const auto& data : kData) {
    const std::wstring path =
        base::StrCat({data.path_prefix, kProductPathName, data.path_suffix});
    EXPECT_EQ(data.install_suffix, GetInstallSuffix(path)) << path;
  }
}

struct TestData {
  const wchar_t* path;
  InstallConstantIndex index;
  bool system_level;
  const wchar_t* channel;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr TestData kTestData[] = {
    {
        L"C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
        STABLE_INDEX,
        true,
        L"",
    },
    {
        L"C:\\Users\\user\\AppData\\Local\\Google\\Chrome\\Application"
        L"\\chrome.exe",
        STABLE_INDEX,
        false,
        L"",
    },
    {
        L"C:\\Program Files (x86)\\Google\\Chrome "
        L"Beta\\Application\\chrome.exe",
        BETA_INDEX,
        true,
        L"beta",
    },
    {
        L"C:\\Users\\user\\AppData\\Local\\Google\\Chrome Beta\\Application"
        L"\\chrome.exe",
        BETA_INDEX,
        false,
        L"beta",
    },
    {
        L"C:\\Program Files (x86)\\Google\\Chrome Dev\\Application\\chrome.exe",
        DEV_INDEX,
        true,
        L"dev",
    },
    {
        L"C:\\Users\\user\\AppData\\Local\\Google\\Chrome Dev\\Application"
        L"\\chrome.exe",
        DEV_INDEX,
        false,
        L"dev",
    },
    {
        L"C:\\Users\\user\\AppData\\Local\\Google\\Chrome SxS\\Application"
        L"\\chrome.exe",
        CANARY_INDEX,
        false,
        L"canary",
    },
    {
        L"C:\\Users\\user\\AppData\\Local\\Google\\CHROME SXS\\application"
        L"\\chrome.exe",
        CANARY_INDEX,
        false,
        L"canary",
    },
};
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
constexpr TestData kTestData[] = {
    {
        L"C:\\Users\\user\\AppData\\Local\\Google\\Chrome for "
        L"Testing\\Application\\chrome.exe",
        GOOGLE_CHROME_FOR_TESTING_INDEX,
        false,
        L"",
    },
};
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr TestData kTestData[] = {
    {
        L"C:\\Program Files (x86)\\Chromium\\Application\\chrome.exe",
        CHROMIUM_INDEX,
        true,
        L"",
    },
    {
        L"C:\\Users\\user\\AppData\\Local\\Chromium\\Application\\chrome.exe",
        CHROMIUM_INDEX,
        false,
        L"",
    },
};
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

// Test that MakeProductDetails properly sniffs out an install's details.
class MakeProductDetailsTest : public testing::TestWithParam<TestData> {
 public:
  MakeProductDetailsTest(const MakeProductDetailsTest&) = delete;
  MakeProductDetailsTest& operator=(const MakeProductDetailsTest&) = delete;

 protected:
  MakeProductDetailsTest()
      : test_data_(GetParam()),
        root_key_(test_data_.system_level ? HKEY_LOCAL_MACHINE
                                          : HKEY_CURRENT_USER),
        nt_root_key_(test_data_.system_level ? nt::HKLM : nt::HKCU) {}

  ~MakeProductDetailsTest() {
    nt::SetTestingOverride(nt_root_key_, std::wstring());
  }

  void SetUp() override {
    std::wstring path;
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(root_key_, &path));
    nt::SetTestingOverride(nt_root_key_, path);
  }

  const TestData& test_data() const { return test_data_; }

  void SetChannelOverride(const wchar_t* value) {
    ASSERT_THAT(base::win::RegKey(root_key_, GetClientsKeyPath().c_str(),
                                  KEY_WOW64_32KEY | KEY_SET_VALUE)
                    .WriteValue(L"channel", value),
                Eq(ERROR_SUCCESS));
  }

  void SetAp(const wchar_t* value) {
    ASSERT_THAT(base::win::RegKey(root_key_, GetClientStateKeyPath().c_str(),
                                  KEY_WOW64_32KEY | KEY_SET_VALUE)
                    .WriteValue(L"ap", value),
                Eq(ERROR_SUCCESS));
  }

  void SetCohortName(const wchar_t* value) {
    ASSERT_THAT(
        base::win::RegKey(root_key_,
                          GetClientStateKeyPath().append(L"\\cohort").c_str(),
                          KEY_WOW64_32KEY | KEY_SET_VALUE)
            .WriteValue(L"name", value),
        Eq(ERROR_SUCCESS));
  }

 private:
  // Returns the registry path for the product's Clients key.
  std::wstring GetClientsKeyPath() {
    std::wstring result(L"Software\\");
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
    result.append(L"Google\\Update\\Clients\\");
    result.append(kInstallModes[test_data().index].app_guid);
#else
    result.append(kProductPathName);
#endif
    return result;
  }

  // Returns the registry path for the product's ClientState key.
  std::wstring GetClientStateKeyPath() {
    std::wstring result(L"Software\\");
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
    result.append(L"Google\\Update\\ClientState\\");
    result.append(kInstallModes[test_data().index].app_guid);
#else
    result.append(kProductPathName);
#endif
    return result;
  }

  registry_util::RegistryOverrideManager override_manager_;
  const TestData& test_data_;
  HKEY root_key_;
  nt::ROOT_KEY nt_root_key_;
};

// Test that the install mode is sniffed properly based on the path.
TEST_P(MakeProductDetailsTest, Index) {
  std::unique_ptr<PrimaryInstallDetails> details(
      MakeProductDetails(test_data().path));
  EXPECT_THAT(details->install_mode_index(), Eq(test_data().index));
}

// Test that user/system level is sniffed properly based on the path.
TEST_P(MakeProductDetailsTest, SystemLevel) {
  std::unique_ptr<PrimaryInstallDetails> details(
      MakeProductDetails(test_data().path));
  EXPECT_THAT(details->system_level(), Eq(test_data().system_level));
}

// Test that the default channel is sniffed properly based on the path.
TEST_P(MakeProductDetailsTest, DefaultChannel) {
  std::unique_ptr<PrimaryInstallDetails> details(
      MakeProductDetails(test_data().path));
  EXPECT_THAT(details->channel(), StrEq(test_data().channel));
}

// Test that the default channel is sniffed properly based on the channel
// override.
TEST_P(MakeProductDetailsTest, PolicyOverrideChannel) {
  static constexpr std::tuple<const wchar_t*, const wchar_t*, bool>
      kChannelOverrides[] = {
          {L"", L"", false},         {L"stable", L"", false},
          {L"extended", L"", true},  {L"dev", L"dev", false},
          {L"beta", L"beta", false},
      };
  for (const auto& the_override : kChannelOverrides) {
    const wchar_t* channel_override;
    const wchar_t* expected_channel;
    bool extended_stable;

    std::tie(channel_override, expected_channel, extended_stable) =
        the_override;
    if (channel_override)
      SetChannelOverride(channel_override);

    std::unique_ptr<PrimaryInstallDetails> details(
        MakeProductDetails(test_data().path));
    switch (kInstallModes[test_data().index].channel_strategy) {
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
      case ChannelStrategy::FLOATING:
        EXPECT_THAT(details->channel(), StrEq(expected_channel));
        EXPECT_THAT(details->channel_origin(), Eq(ChannelOrigin::kPolicy));
        EXPECT_THAT(details->channel_override(), StrEq(channel_override));
        EXPECT_THAT(details->is_extended_stable_channel(), Eq(extended_stable));
        break;
      case ChannelStrategy::FIXED:
#else   // BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
      case ChannelStrategy::UNSUPPORTED:
#endif  // BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
        // The override is ignored for this mode.
        EXPECT_THAT(details->channel(), StrEq(test_data().channel));
        break;
    }
  }
}

// Test that the "ap" value is cached during initialization.
TEST_P(MakeProductDetailsTest, UpdateAp) {
  // This test is only valid for brands that integrate with Google Update.
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  // With no value in the registry, the ap value should be empty.
  {
    std::unique_ptr<PrimaryInstallDetails> details(
        MakeProductDetails(test_data().path));
    EXPECT_THAT(details->update_ap(), StrEq(L""));
  }

  // And with a value, it should have ... the value.
  static constexpr wchar_t kCrookedMoon[] = L"CrookedMoon";
  SetAp(kCrookedMoon);
  {
    std::unique_ptr<PrimaryInstallDetails> details(
        MakeProductDetails(test_data().path));
    EXPECT_THAT(details->update_ap(), StrEq(kCrookedMoon));
  }
#endif
}

// Test that the cohort name is cached during initialization.
TEST_P(MakeProductDetailsTest, UpdateCohortName) {
  // This test is only valid for brands that integrate with Google Update.
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  // With no value in the registry, the cohort name should be empty.
  {
    std::unique_ptr<PrimaryInstallDetails> details(
        MakeProductDetails(test_data().path));
    EXPECT_THAT(details->update_cohort_name(), StrEq(L""));
  }

  // And with a value, it should have ... the value.
  static constexpr wchar_t kPhony[] = L"Phony";
  SetCohortName(kPhony);
  {
    std::unique_ptr<PrimaryInstallDetails> details(
        MakeProductDetails(test_data().path));
    EXPECT_THAT(details->update_cohort_name(), StrEq(kPhony));
  }
#endif
}

INSTANTIATE_TEST_SUITE_P(All,
                         MakeProductDetailsTest,
                         testing::ValuesIn(kTestData));

}  // namespace install_static
