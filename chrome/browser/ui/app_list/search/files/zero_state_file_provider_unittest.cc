// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_file_provider.h"

#include <string>

#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using ::file_manager::file_tasks::FileTasksObserver;
using ::testing::UnorderedElementsAre;

MATCHER_P(Title, title, "") {
  return arg->title() == title;
}

}  // namespace

class ZeroStateFileProviderTest : public testing::Test {
 protected:
  ZeroStateFileProviderTest() = default;
  ~ZeroStateFileProviderTest() override = default;

  void SetUp() override {
    app_list_color_provider_ =
        std::make_unique<ash::TestAppListColorProvider>();

    profile_ = std::make_unique<TestingProfile>();

    auto provider = std::make_unique<ZeroStateFileProvider>(profile_.get());
    provider_ = provider.get();
    search_controller_.AddProvider(0, std::move(provider));

    Wait();
  }

  void TearDown() override { app_list_color_provider_.reset(); }

  base::FilePath Path(const std::string& filename) {
    return profile_->GetPath().AppendASCII(filename);
  }

  void WriteFile(const std::string& filename) {
    CHECK(base::WriteFile(Path(filename), "abcd"));
    CHECK(base::PathExists(Path(filename)));
    Wait();
  }

  FileTasksObserver::FileOpenEvent OpenEvent(const std::string& filename) {
    FileTasksObserver::FileOpenEvent e;
    e.path = Path(filename);
    e.open_type = FileTasksObserver::OpenType::kOpen;
    return e;
  }

  void StartSearch(const std::u16string& query) {
    search_controller_.StartSearch(query);
  }

  void StartZeroStateSearch() {
    search_controller_.StartZeroState(base::DoNothing(), base::TimeDelta());
  }

  const SearchProvider::Results& LastResults() {
    return search_controller_.last_results();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<Profile> profile_;
  TestSearchController search_controller_;
  ZeroStateFileProvider* provider_ = nullptr;
  std::unique_ptr<ash::TestAppListColorProvider> app_list_color_provider_;
};

TEST_F(ZeroStateFileProviderTest, NoResultsWithQuery) {
  StartSearch(u"query");
  Wait();
  EXPECT_TRUE(LastResults().empty());
}

TEST_F(ZeroStateFileProviderTest, ResultsProvided) {
  WriteFile("exists_1.txt");
  WriteFile("exists_2.png");
  WriteFile("exists_3.pdf");

  // Results are only added if they have been opened at least once.
  provider_->OnFilesOpened(
      {OpenEvent("exists_1.txt"), OpenEvent("exists_2.png")});
  provider_->OnFilesOpened({OpenEvent("nonexistant.txt")});

  StartZeroStateSearch();
  Wait();

  EXPECT_THAT(LastResults(), UnorderedElementsAre(Title(u"exists_1.txt"),
                                                  Title(u"exists_2.png")));
}

TEST_F(ZeroStateFileProviderTest, OldFilesNotReturned) {
  WriteFile("new.txt");
  WriteFile("old.png");
  auto now = base::Time::Now();
  base::TouchFile(Path("old.png"), now, now - base::Days(8));

  provider_->OnFilesOpened({OpenEvent("new.txt"), OpenEvent("old.png")});

  StartZeroStateSearch();
  Wait();

  EXPECT_THAT(LastResults(), UnorderedElementsAre(Title(u"new.txt")));
}

}  // namespace app_list
