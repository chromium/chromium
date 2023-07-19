// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/msi_tag_extractor.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/updater/util/unit_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

class MsiTagExtractorTest : public testing::Test {
 protected:
  void SetUp() override {
    tagged_msi_path_ = test::GetTestFilePath("tagged_msi");
    ASSERT_TRUE(base::DirectoryExists(tagged_msi_path_));
  }

  base::FilePath GetMsiFilePath(
      const base::FilePath::StringType& file_name) const {
    const base::FilePath msi_file_path = tagged_msi_path_.Append(file_name);
    EXPECT_TRUE(base::PathExists(msi_file_path));
    return msi_file_path;
  }

 private:
  base::FilePath tagged_msi_path_;
};

TEST_F(MsiTagExtractorTest, ExtractTagMap) {
  const struct {
    const wchar_t* msi_file_name;
    const base::flat_map<std::string, std::string> expected_tag_map;
  } test_cases[] = {
      // tag:BRAND=QAQA.
      {L"GUH-brand-only.msi", {{"brand", "QAQA"}}},

      // tag:BRAND=QAQA&.
      {L"GUH-ampersand-ending.msi", {{"brand", "QAQA"}}},

      // tag:
      //   appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&
      //   iid={2D8C18E9-8D3A-4EFC-6D61-AE23E3530EA2}&
      //   lang=en&browser=4&usagestats=0&appname=Google%20Chrome&
      //   needsadmin=prefers&brand=CHMB&
      //   installdataindex=defaultbrowser.
      {L"GUH-multiple.msi",
       {{"appguid", "{8A69D345-D564-463C-AFF1-A69D9E530F96}"},
        {"iid", "{2D8C18E9-8D3A-4EFC-6D61-AE23E3530EA2}"},
        {"lang", "en"},
        {"browser", "4"},
        {"usagestats", "0"},
        {"appname", "Google%20Chrome"},
        {"needsadmin", "prefers"},
        {"brand", "CHMB"},
        {"installdataindex", "defaultbrowser"}}},

      // tag: =value&BRAND=QAQA.
      {L"GUH-empty-key.msi", {{"brand", "QAQA"}}},

      // tag: =value&BRAND=QAQA.
      {L"GUH-empty-value.msi", {{"brand", ""}}},

      // tag:(empty string).
      {L"GUH-empty-tag.msi", {}},

      // invalid magic signature "Gact2.0Foo".
      {L"GUH-invalid-marker.msi", {}},

      // invalid characters in the tag key.
      {L"GUH-invalid-key.msi", {}},

      // invalid characters in the tag value.
      {L"GUH-invalid-value.msi", {}},

      // invalid tag format.
      {L"GUH-bad-format.msi", {}},

      // invalid tag format.
      {L"GUH-bad-format2.msi", {}},
  };

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(ExtractTagMap(GetMsiFilePath(test_case.msi_file_name)),
              test_case.expected_tag_map)
        << test_case.msi_file_name;
  }
}

}  // namespace updater
