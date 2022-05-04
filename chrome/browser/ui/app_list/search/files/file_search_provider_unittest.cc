// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_search_provider.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"
#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using ::testing::ElementsAre;
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
    search_controller_ = std::make_unique<TestSearchController>();
    provider_ = std::make_unique<FileSearchProvider>(profile_.get());

    provider_->set_controller(search_controller_.get());

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

  const SearchProvider::Results& LastResults() {
    if (app_list_features::IsCategoricalSearchEnabled()) {
      return search_controller_->last_results();
    } else {
      return provider_->results();
    }
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<Profile> profile_;
  std::unique_ptr<ash::TestAppListColorProvider> app_list_color_provider_;
  std::unique_ptr<TestSearchController> search_controller_;
  std::unique_ptr<FileSearchProvider> provider_;
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(FileSearchProviderTest, SearchResultsMatchQuery) {
  WriteFile("file_1.txt");
  WriteFile("no_match.png");
  WriteFile("my_file_2.png");

  provider_->Start(u"file");
  Wait();

  EXPECT_THAT(LastResults(), UnorderedElementsAre(Title("file_1.txt"),
                                                  Title("my_file_2.png")));
}

TEST_F(FileSearchProviderTest, SearchIsCaseInsensitive) {
  WriteFile("FILE_1.png");
  WriteFile("FiLe_2.Png");

  provider_->Start(u"fIle");
  Wait();

  EXPECT_THAT(LastResults(),
              UnorderedElementsAre(Title("FILE_1.png"), Title("FiLe_2.Png")));
}

TEST_F(FileSearchProviderTest, SearchDirectories) {
  CreateDirectory("my_folder");

  provider_->Start(u"my_folder");
  Wait();

  EXPECT_THAT(LastResults(), UnorderedElementsAre(Title("my_folder")));
}

TEST_F(FileSearchProviderTest, ResultMetadataTest) {
  WriteFile("file.txt");

  provider_->Start(u"file");
  Wait();

  ASSERT_TRUE(LastResults().size() == 1u);
  const auto& result = LastResults()[0];
  EXPECT_EQ(result->result_type(), ash::AppListSearchResultType::kFileSearch);
  EXPECT_EQ(result->display_type(), ash::SearchResultDisplayType::kList);
}

TEST_F(FileSearchProviderTest, RecentlyAccessedFilesHaveHigherRelevance) {
  WriteFile("file.txt");
  WriteFile("file.png");
  WriteFile("file.pdf");

  // Set the access times of all files to be different.
  const base::Time time = base::Time::Now();
  const base::Time earlier_time = time - base::Days(5);
  const base::Time earliest_time = time - base::Days(10);
  TouchFile(Path("file.txt"), time, time);
  TouchFile(Path("file.png"), earliest_time, time);
  TouchFile(Path("file.pdf"), earlier_time, time);

  provider_->Start(u"file");
  Wait();

  ASSERT_TRUE(LastResults().size() == 3u);

  // Sort the results by descending relevance.
  std::vector<ChromeSearchResult*> results;
  for (const auto& result : LastResults()) {
    results.push_back(result.get());
  }
  std::sort(results.begin(), results.end(),
            [](const ChromeSearchResult* a, const ChromeSearchResult* b) {
              return a->relevance() > b->relevance();
            });
  ASSERT_TRUE(results[0]->relevance() > results[1]->relevance());
  ASSERT_TRUE(results[1]->relevance() > results[2]->relevance());

  // Most recently accessed files should be at the front.
  EXPECT_THAT(results, ElementsAre(Title("file.txt"), Title("file.pdf"),
                                   Title("file.png")));
}

TEST_F(FileSearchProviderTest, HighScoringFilesHaveScoreInRightRange) {
  // Make two identically named files with different access times.
  const base::Time time = base::Time::Now();
  const base::Time earlier_time = time - base::Days(5);
  CreateDirectory("dir");
  WriteFile("dir/file");
  WriteFile("file");
  TouchFile(Path("dir/file"), time, time);
  TouchFile(Path("file"), earlier_time, time);

  // Match them perfectly, so both score 1.0.
  provider_->Start(u"file");
  Wait();

  ASSERT_EQ(LastResults().size(), 2u);

  // Sort the results by descending relevance.
  std::vector<ChromeSearchResult*> results;
  for (const auto& result : LastResults()) {
    results.push_back(result.get());
  }
  std::sort(results.begin(), results.end(),
            [](const ChromeSearchResult* a, const ChromeSearchResult* b) {
              return a->relevance() > b->relevance();
            });
  // The scores should be properly in order and not exceed 1.0.
  EXPECT_GT(results[0]->relevance(), results[1]->relevance());
  EXPECT_LE(results[0]->relevance(), 1.0);
}

}  // namespace app_list
