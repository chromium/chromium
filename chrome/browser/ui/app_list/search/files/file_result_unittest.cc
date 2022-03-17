// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_result.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list {
namespace {

using testing::DoubleNear;

}  // namespace

class FileResultTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
  }

  // Returns a file path with its last_accessed set to |now - accessed_ago|.
  base::FilePath TouchFile(base::TimeDelta accessed_ago) {
    auto now = base::Time::Now();
    base::FilePath path(temp_dir_.GetPath().Append("somefile"));
    base::WriteFile(path, "content");
    base::TouchFile(path, now - accessed_ago, now - accessed_ago);
    return path;
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  ash::TestAppListColorProvider app_list_color_provider_;
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<Profile> profile_;
};

TEST_F(FileResultTest, CheckMetadata) {
  FileResult result(
      "zero_state_file://", base::FilePath("/my/test/MIXED_case_FILE.Pdf"),
      u"some details", ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get());
  EXPECT_EQ(base::UTF16ToUTF8(result.title()),
            std::string("MIXED_case_FILE.Pdf"));
  EXPECT_EQ(result.details(), u"some details");
  EXPECT_EQ(result.id(), "zero_state_file:///my/test/MIXED_case_FILE.Pdf");
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kZeroStateFile);
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kList);
  EXPECT_EQ(result.relevance(), 0.2f);
}

TEST_F(FileResultTest, HostedExtensionsIgnored) {
  FileResult result_1(
      "zero_state_file://", base::FilePath("my/Document.gdoc"), absl::nullopt,
      ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get());
  FileResult result_2(
      "zero_state_file://", base::FilePath("my/Map.gmaps"), absl::nullopt,
      ash::AppListSearchResultType::kZeroStateFile,
      ash::SearchResultDisplayType::kList, 0.2f, std::u16string(),
      FileResult::Type::kFile, profile_.get());

  EXPECT_EQ(base::UTF16ToUTF8(result_1.title()), std::string("Document"));
  EXPECT_EQ(base::UTF16ToUTF8(result_2.title()), std::string("Map"));
}

TEST_F(FileResultTest, PenalizeScore) {
  double expected_score[] = {1.0, 0.9, 0.63};
  int access_days_ago[] = {0, 10, 30};

  for (int i = 0; i < 3; ++i) {
    auto path = TouchFile(base::Days(access_days_ago[i]));
    FileResult result("zero_state_file://", path, absl::nullopt,
                      ash::AppListSearchResultType::kZeroStateFile,
                      ash::SearchResultDisplayType::kList, 1.0,
                      std::u16string(), FileResult::Type::kFile,
                      profile_.get());
    result.PenalizeRelevanceByAccessTime();
    Wait();

    EXPECT_THAT(result.relevance(), DoubleNear(expected_score[i], 0.01));
  }
}

}  // namespace app_list
