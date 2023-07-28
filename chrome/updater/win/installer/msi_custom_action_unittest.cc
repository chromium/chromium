// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/msi_custom_action.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "chrome/updater/util/unit_test_util.h"
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
};

}  // namespace

TEST(MsiCustomActionTest, MsiSetTags) {
  const struct {
    const std::string msi_file_name;
    const std::string expected_tag_string;
  } test_cases[] = {
      // single tag parameter.
      {"GUH-brand-only.msi", "BRAND=QAQA"},

      // single tag parameter ending in an ampersand.
      {"GUH-ampersand-ending.msi", "BRAND=QAQA"},

      // multiple tag parameters.
      {"GUH-multiple.msi",
       "APPGUID={8A69D345-D564-463C-AFF1-A69D9E530F96}&IID={2D8C18E9-8D3A-4EFC-"
       "6D61-AE23E3530EA2}&LANG=en&BROWSER=4&USAGESTATS=0&APPNAME=Google "
       "Chrome&NEEDSADMIN=prefers&BRAND=CHMB&INSTALLDATAINDEX=defaultbrowser"},

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
  };

  for (const auto& test_case : test_cases) {
    MockMsiHandle mock_msi_handle;
    const std::wstring msi_file_path = test::GetTestFilePath("tagged_msi")
                                           .AppendASCII(test_case.msi_file_name)
                                           .value();
    EXPECT_CALL(mock_msi_handle,
                GetProperty(std::wstring(L"OriginalDatabase"), _, Eq(1U)))
        .WillOnce(DoAll(SetArgReferee<2>(msi_file_path.length()),
                        Return(ERROR_MORE_DATA)));
    EXPECT_CALL(mock_msi_handle,
                GetProperty(std::wstring(L"OriginalDatabase"), _,
                            Eq(msi_file_path.length() + 1U)))
        .WillOnce(DoAll(SetArgReferee<1>(std::vector(msi_file_path.begin(),
                                                     msi_file_path.end())),
                        SetArgReferee<2>(msi_file_path.length()),
                        Return(ERROR_SUCCESS)));
    std::string tag_string;
    EXPECT_CALL(mock_msi_handle, SetProperty)
        .WillRepeatedly(DoAll(
            Invoke([&](const std::string& name, const std::string& value) {
              tag_string += base::StrCat(
                  {!tag_string.empty() ? "&" : "", name, "=", value});
            }),
            Return(ERROR_SUCCESS)));

    MsiSetTags(mock_msi_handle);
    EXPECT_EQ(tag_string, test_case.expected_tag_string);
  }
}

TEST(MsiCustomActionTest, ExtractTagInfoFromInstaller) {
  EXPECT_EQ(ExtractTagInfoFromInstaller(0), static_cast<UINT>(ERROR_SUCCESS));
}

}  // namespace updater
