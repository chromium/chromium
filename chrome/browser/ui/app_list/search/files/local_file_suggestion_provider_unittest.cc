// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/local_file_suggestion_provider.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/file_tasks_observer.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using file_manager::file_tasks::FileTasksObserver;
using testing::UnorderedElementsAre;

FileTasksObserver::FileOpenEvent OpenEvent(const base::FilePath& path) {
  FileTasksObserver::FileOpenEvent e;
  e.path = path;
  e.open_type = FileTasksObserver::OpenType::kOpen;
  return e;
}

MATCHER_P(FilePathMatcher, file_path, "") {
  return arg.file_path == file_path;
}

}  // namespace

class LocalFileSuggestionProviderTest : public testing::Test {
 public:
  void OnSuggestionsUpdated(FileSuggestionType type) {
    ASSERT_TRUE(type == FileSuggestionType::kLocalFile);
  }

  base::FilePath Path(const std::string& filename) {
    return profile_->GetPath().AppendASCII(filename);
  }

  void WriteFile(const base::FilePath& path) {
    CHECK(base::WriteFile(path, "abcd"));
    CHECK(base::PathExists(path));
    Wait();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  void WaitForProviderToBeInitialized() {
    while (!provider_->IsInitialized())
      Wait();
  }

  void UpdateResults() {
    base::RunLoop run_loop;
    auto cb = base::BindLambdaForTesting(
        [&](const absl::optional<std::vector<FileSuggestData>>& data) {
          results_ = data;
          run_loop.Quit();
        });
    provider_->GetSuggestFileData(std::move(cb));
    run_loop.Run();
  }

  absl::optional<std::vector<FileSuggestData>>& Results() { return results_; }

  LocalFileSuggestionProvider* GetProvider() { return provider_.get(); }

 protected:
  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(
        "primary_profile@test", {});
    provider_ = std::make_unique<LocalFileSuggestionProvider>(
        profile_, base::BindRepeating(
                      &LocalFileSuggestionProviderTest::OnSuggestionsUpdated,
                      base::Unretained(this)));
    UpdateResults();
    WaitForProviderToBeInitialized();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  TestingProfile* profile_;
  std::unique_ptr<LocalFileSuggestionProvider> provider_;
  absl::optional<std::vector<FileSuggestData>> results_;
};

TEST_F(LocalFileSuggestionProviderTest, ResultsEmptyOnInitialization) {
  // The results fetched before initialization should have no value.
  EXPECT_FALSE(Results().has_value());

  // Now that the provider is initialized, results should be empty but not null.
  UpdateResults();
  ASSERT_TRUE(Results().has_value());
  EXPECT_EQ(Results()->size(), 0u);
}

TEST_F(LocalFileSuggestionProviderTest, ResultsAreOnlyOpenedFiles) {
  WriteFile(Path("exists_1.txt"));
  WriteFile(Path("exists_2.png"));
  WriteFile(Path("exists_3.pdf"));

  // Results are only added if they exist and have been opened at least once.
  GetProvider()->OnFilesOpened({OpenEvent(Path("exists_1.txt")),
                                OpenEvent(Path("exists_2.png")),
                                OpenEvent(Path("nonexistent.txt"))});

  UpdateResults();

  ASSERT_TRUE(Results().has_value());
  EXPECT_THAT(Results().value(),
              UnorderedElementsAre(FilePathMatcher(Path("exists_1.txt")),
                                   FilePathMatcher(Path("exists_2.png"))));
}

TEST_F(LocalFileSuggestionProviderTest, OldFilesNotReturned) {
  WriteFile(Path("new.txt"));
  WriteFile(Path("old.png"));
  auto now = base::Time::Now();
  base::TouchFile(
      Path("old.png"), now,
      now -
          base::Days(
              LocalFileSuggestionProvider::kDefaultMaxLastModifiedTimeInDays));

  GetProvider()->OnFilesOpened(
      {OpenEvent(Path("new.txt")), OpenEvent(Path("old.png"))});

  UpdateResults();

  ASSERT_TRUE(Results().has_value());
  EXPECT_THAT(Results().value(),
              UnorderedElementsAre(FilePathMatcher(Path("new.txt"))));
}

}  // namespace app_list
