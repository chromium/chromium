// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_search_provider.h"

#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using ::testing::UnorderedElementsAre;

MATCHER_P(Title, title, "") {
  return base::UTF16ToUTF8(arg->title()) == title;
}

}  // namespace

class FileSearchProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    app_list_color_provider_ =
        std::make_unique<ash::TestAppListColorProvider>();
    provider_ = std::make_unique<FileSearchProvider>(profile_.get());

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    provider_->SetRootPathForTesting(scoped_temp_dir_.GetPath());

    Wait();
  }

  base::FilePath Path(const std::string& filename) {
    return scoped_temp_dir_.GetPath().AppendASCII(filename);
  }

  void WriteFile(const std::string& filename) {
    ASSERT_TRUE(base::WriteFile(Path(filename), "abcd"));
    ASSERT_TRUE(base::PathExists(Path(filename)));
    Wait();
  }

  void CreateDirectory(const std::string& directory_name) {
    ASSERT_TRUE(base::CreateDirectory(Path(directory_name)));
    ASSERT_TRUE(base::PathExists(Path(directory_name)));
    Wait();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<Profile> profile_;
  std::unique_ptr<ash::TestAppListColorProvider> app_list_color_provider_;
  std::unique_ptr<FileSearchProvider> provider_;
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(FileSearchProviderTest, NoResultsInZeroState) {
  WriteFile("file.txt");

  provider_->Start(u"");
  Wait();

  EXPECT_TRUE(provider_->results().empty());
}

TEST_F(FileSearchProviderTest, SearchResultsMatchQuery) {
  WriteFile("file_1.txt");
  WriteFile("no_match.png");
  WriteFile("my_file_2.png");

  provider_->Start(u"file");
  Wait();

  EXPECT_THAT(
      provider_->results(),
      UnorderedElementsAre(Title("file_1.txt"), Title("my_file_2.png")));
}

TEST_F(FileSearchProviderTest, SearchIsCaseInsensitive) {
  WriteFile("FILE_1.png");
  WriteFile("FiLe_2.Png");

  provider_->Start(u"fIle");
  Wait();

  EXPECT_THAT(provider_->results(),
              UnorderedElementsAre(Title("FILE_1.png"), Title("FiLe_2.Png")));
}

TEST_F(FileSearchProviderTest, DirectoriesIgnored) {
  CreateDirectory("my_folder");

  provider_->Start(u"my_folder");
  Wait();

  EXPECT_TRUE(provider_->results().empty());
}

TEST_F(FileSearchProviderTest, ResultMetadataTest) {
  WriteFile("file.txt");

  provider_->Start(u"file");
  Wait();

  ASSERT_TRUE(provider_->results().size() == 1u);
  const auto& result = provider_->results()[0];
  EXPECT_EQ(result->result_type(), ash::AppListSearchResultType::kFileSearch);
  EXPECT_EQ(result->display_type(), ash::SearchResultDisplayType::kList);
}

}  // namespace app_list
