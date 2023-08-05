// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/msi_custom_action.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/updater/util/unit_test_util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/installer_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

class MockMsiHandle : public MsiHandleInterface {
 public:
  ~MockMsiHandle() override = default;
  MOCK_METHOD(UINT,
              GetProperty,
              (const std::wstring& name,
               std::vector<wchar_t>& value,
               DWORD& value_length),
              (const, override));
  MOCK_METHOD(UINT,
              SetProperty,
              (const std::string& name, const std::string& value),
              (override));
  MOCK_METHOD(MSIHANDLE, CreateRecord, (UINT field_count), (override));
  MOCK_METHOD(UINT,
              RecordSetString,
              (MSIHANDLE record_handle,
               UINT field_index,
               const std::wstring& value),
              (override));
  MOCK_METHOD(int,
              ProcessMessage,
              (INSTALLMESSAGE message_type, MSIHANDLE record_handle),
              (override));
};

struct MsiSetTagsTestCase {
  const std::string msi_file_name;
  const std::string expected_tag_string;
};

}  // namespace

class MsiSetTagsTest : public TestWithParam<MsiSetTagsTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    MsiSetTagsTestCases,
    MsiSetTagsTest,
    ValuesIn(std::vector<MsiSetTagsTestCase>{
        // single tag parameter.
        {"GUH-brand-only.msi", "BRAND=QAQA"},

        // single tag parameter ending in an ampersand.
        {"GUH-ampersand-ending.msi", "BRAND=QAQA"},

        // multiple tag parameters.
        {"GUH-multiple.msi",
         "APPGUID={8A69D345-D564-463C-AFF1-A69D9E530F96}&IID={2D8C18E9-8D3A-"
         "4EFC-"
         "6D61-AE23E3530EA2}&LANG=en&BROWSER=4&USAGESTATS=0&APPNAME=Google "
         "Chrome&NEEDSADMIN=prefers&BRAND=CHMB&INSTALLDATAINDEX="
         "defaultbrowser"},

        // special character in the tag value.
        {"GUH-special-value.msi", "BRAND=QA*A"},

        // untagged msi.
        {"GUH-untagged.msi", {}},

        // invalid magic signature "Gact2.0Foo".
        {"GUH-invalid-marker.msi", {}},

        // invalid characters in the tag key.
        {"GUH-invalid-key.msi", {}},

        // invalid tag format.
        {"GUH-bad-format.msi", {}},

        // invalid tag format.
        {"GUH-bad-format2.msi", {}},

        // untagged.
        {"GUH-untagged.msi", {}},
    }));

TEST_P(MsiSetTagsTest, MsiSetTags) {
  MockMsiHandle mock_msi_handle;
  const std::wstring msi_file_path = test::GetTestFilePath("tagged_msi")
                                         .AppendASCII(GetParam().msi_file_name)
                                         .value();
  EXPECT_CALL(mock_msi_handle,
              GetProperty(std::wstring(L"OriginalDatabase"), _, Eq(1U)))
      .WillOnce(DoAll(SetArgReferee<2>(msi_file_path.length()),
                      Return(ERROR_MORE_DATA)));
  EXPECT_CALL(mock_msi_handle, GetProperty(std::wstring(L"OriginalDatabase"), _,
                                           Eq(msi_file_path.length() + 1U)))
      .WillOnce(DoAll(SetArgReferee<1>(std::vector(msi_file_path.begin(),
                                                   msi_file_path.end())),
                      SetArgReferee<2>(msi_file_path.length()),
                      Return(ERROR_SUCCESS)));
  std::string tag_string;
  EXPECT_CALL(mock_msi_handle, SetProperty)
      .WillRepeatedly(
          DoAll(Invoke([&](const std::string& name, const std::string& value) {
                  tag_string += base::StrCat(
                      {!tag_string.empty() ? "&" : "", name, "=", value});
                }),
                Return(ERROR_SUCCESS)));

  MsiSetTags(mock_msi_handle);
  EXPECT_EQ(tag_string, GetParam().expected_tag_string);
}

TEST(MsiCustomActionTest, ExtractTagInfoFromInstaller) {
  EXPECT_EQ(ExtractTagInfoFromInstaller(0), static_cast<UINT>(ERROR_SUCCESS));
}

class MsiSetInstallerResultTest
    : public ::testing::TestWithParam<std::tuple<bool, bool, bool>> {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    if (SetResults()) {
      InstallerOutcome installer_outcome = {};
      installer_outcome.installer_result = InstallerResult::kCustomError;
      installer_outcome.installer_text = "some text";
      EXPECT_TRUE(SetInstallerOutcomeForTesting(
          UpdaterScope::kSystem, base::WideToASCII(kAppId), installer_outcome));
      ASSERT_TRUE(GetInstallerOutcome(UpdaterScope::kSystem,
                                      base::WideToASCII(kAppId)));

      if (OnlyInUpdaterKey()) {
        EXPECT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE, L"", Wow6432(KEY_WRITE))
                      .DeleteKey(GetAppClientStateKey(kAppId).c_str()),
                  ERROR_SUCCESS);
      }
    }
  }

  bool SetResults() const { return std::get<0>(GetParam()); }
  bool OnlyInUpdaterKey() const { return std::get<1>(GetParam()); }
  bool ValidCustomActionData() const { return std::get<2>(GetParam()); }

  const std::wstring kAppId = L"{55d6c27c-8b97-4b76-a691-2df8810004ed}";
  registry_util::RegistryOverrideManager registry_override_manager_;
};

INSTANTIATE_TEST_SUITE_P(SetResults_OnlyInUpdaterKey_ValidCustomActionData,
                         MsiSetInstallerResultTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

TEST_P(MsiSetInstallerResultTest, MsiSetInstallerResult) {
  MockMsiHandle mock_msi_handle;
  if (ValidCustomActionData()) {
    EXPECT_CALL(mock_msi_handle,
                GetProperty(std::wstring(L"CustomActionData"), _, Eq(1U)))
        .WillOnce(
            DoAll(SetArgReferee<2>(kAppId.length()), Return(ERROR_MORE_DATA)));
    EXPECT_CALL(mock_msi_handle, GetProperty(std::wstring(L"CustomActionData"),
                                             _, Eq(kAppId.length() + 1U)))
        .WillOnce(
            DoAll(SetArgReferee<1>(std::vector(kAppId.begin(), kAppId.end())),
                  SetArgReferee<2>(kAppId.length()), Return(ERROR_SUCCESS)));
  } else {
    EXPECT_CALL(mock_msi_handle,
                GetProperty(std::wstring(L"CustomActionData"), _, Eq(1U)))
        .WillOnce(Return(ERROR_SUCCESS));
  }
  if (ValidCustomActionData() && SetResults()) {
    EXPECT_CALL(mock_msi_handle, CreateRecord(0)).WillOnce(Return(33));
    EXPECT_CALL(mock_msi_handle,
                RecordSetString(33, 0, std::wstring(L"some text")))
        .WillOnce(Return(ERROR_SUCCESS));
    EXPECT_CALL(mock_msi_handle, ProcessMessage(INSTALLMESSAGE_ERROR, 33))
        .WillOnce(Return(ERROR_SUCCESS));
  }

  MsiSetInstallerResult(mock_msi_handle);
}

TEST(MsiCustomActionTest, ShowInstallerResultUIString) {
  EXPECT_EQ(ShowInstallerResultUIString(0), static_cast<UINT>(ERROR_SUCCESS));
}

}  // namespace updater
