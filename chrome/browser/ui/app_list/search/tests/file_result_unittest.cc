// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_result.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class FileResultTest : public testing::Test {
 public:
  FileResultTest() {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
  }

  ~FileResultTest() override = default;

  ash::TestAppListColorProvider app_list_color_provider_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<Profile> profile_;
};

TEST_F(FileResultTest, CheckMetadata) {
  FileResult result("zero_state_file://",
                    base::FilePath("/my/test/MIXED_case_FILE.Pdf"),
                    ash::AppListSearchResultType::kZeroStateFile,
                    ash::SearchResultDisplayType::kList, 0.2f, profile_.get());
  EXPECT_EQ(base::UTF16ToUTF8(result.title()),
            std::string("MIXED_case_FILE.Pdf"));
  EXPECT_EQ(result.id(), "zero_state_file:///my/test/MIXED_case_FILE.Pdf");
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kZeroStateFile);
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kList);
  EXPECT_EQ(result.relevance(), 0.2f);
}

TEST_F(FileResultTest, HostedExtensionsIgnored) {
  FileResult result_1("zero_state_file://", base::FilePath("my/Document.gdoc"),
                      ash::AppListSearchResultType::kZeroStateFile,
                      ash::SearchResultDisplayType::kList, 0.2f,
                      profile_.get());
  FileResult result_2("zero_state_file://", base::FilePath("my/Map.gmaps"),
                      ash::AppListSearchResultType::kZeroStateFile,
                      ash::SearchResultDisplayType::kList, 0.2f,
                      profile_.get());

  EXPECT_EQ(base::UTF16ToUTF8(result_1.title()), std::string("Document"));
  EXPECT_EQ(base::UTF16ToUTF8(result_2.title()), std::string("Map"));
}

}  // namespace app_list
