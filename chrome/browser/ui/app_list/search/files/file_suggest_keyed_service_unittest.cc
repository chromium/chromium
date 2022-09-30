// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_test_util.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"
#include "chrome/browser/ui/app_list/search/files/mock_file_suggest_keyed_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class FileSuggestKeyedServiceTest : public testing::Test {
 protected:
  // testing::Test:
  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(
        "primary_profile@test", GetTestingFactories());
    WaitUntilFileSuggestServiceReady(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile_));
  }

  virtual TestingProfile::TestingFactories GetTestingFactories() { return {}; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  TestingProfile* profile_ = nullptr;
};

TEST_F(FileSuggestKeyedServiceTest, GetSuggestData) {
  base::HistogramTester tester;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile_)
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce([](const absl::optional<std::vector<FileSuggestData>>&
                                suggest_data) {
            EXPECT_FALSE(suggest_data.has_value());
          }));
  tester.ExpectBucketCount(
      "Ash.Search.DriveFileSuggestDataValidation.Status",
      /*sample=*/DriveSuggestValidationStatus::kDriveFSNotMounted,
      /*expected_count=*/1);
}

class FileSuggestKeyedServiceRemoveTest : public FileSuggestKeyedServiceTest {
 protected:
  // FileSuggestKeyedServiceTest:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    FileSuggestKeyedServiceTest::SetUp();
    file_suggest_service_ = static_cast<MockFileSuggestKeyedService*>(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile_));
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{FileSuggestKeyedServiceFactory::GetInstance(),
             base::BindRepeating(
                 &MockFileSuggestKeyedService::BuildMockFileSuggestKeyedService,
                 temp_dir_.GetPath().Append("proto"))}};
  }

  absl::optional<std::vector<FileSuggestData>> GetSuggestionsForType(
      FileSuggestionType type) {
    absl::optional<std::vector<FileSuggestData>> suggestions;
    base::RunLoop run_loop;
    file_suggest_service_->GetSuggestFileData(
        type, base::BindOnce(
                  [](base::RunLoop* run_loop,
                     absl::optional<std::vector<FileSuggestData>>* suggestions,
                     const absl::optional<std::vector<FileSuggestData>>&
                         fetched_suggestions) {
                    *suggestions = fetched_suggestions;
                    run_loop->Quit();
                  },
                  &run_loop, &suggestions));
    run_loop.Run();
    return suggestions;
  }

  // Hosts the proto file.
  base::ScopedTempDir temp_dir_;

  // This test verifies the suggestion removal only. Therefore, a mock file
  // suggest keyed service is sufficient.
  MockFileSuggestKeyedService* file_suggest_service_ = nullptr;
};

// Verifies that a removed suggestion is not fetchable.
TEST_F(FileSuggestKeyedServiceRemoveTest, SuggestionRemoval) {
  const base::FilePath path1("file1");
  const base::FilePath path2("file2");
  file_suggest_service_->SetSuggestionsForType(
      /*type=*/FileSuggestionType::kDriveFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kDriveFile, path1,
           /*new_prediction_reason=*/absl::nullopt,
           /*new_score=*/absl::nullopt},
          {FileSuggestionType::kDriveFile, path2,
           /*new_prediction_reason=*/absl::nullopt,
           /*new_score=*/absl::nullopt}});

  absl::optional<std::vector<FileSuggestData>> suggestions =
      GetSuggestionsForType(FileSuggestionType::kDriveFile);
  EXPECT_EQ(2u, suggestions->size());
  EXPECT_EQ("file1", suggestions->at(0).file_path.value());
  EXPECT_EQ("zero_state_drive://file1", suggestions->at(0).id);
  EXPECT_EQ("file2", suggestions->at(1).file_path.value());
  EXPECT_EQ("zero_state_drive://file2", suggestions->at(1).id);

  file_suggest_service_->RemoveFileSuggestion(
      /*type=*/FileSuggestionType::kDriveFile,
      /*suggestion_id=*/"zero_state_drive://" + path1.value());

  suggestions = GetSuggestionsForType(FileSuggestionType::kDriveFile);
  EXPECT_EQ(1u, suggestions->size());
  EXPECT_EQ("file2", suggestions->at(0).file_path.value());
  EXPECT_EQ("zero_state_drive://file2", suggestions->at(0).id);

  const base::FilePath path3("file3");
  const base::FilePath path4("file4");
  file_suggest_service_->SetSuggestionsForType(
      /*type=*/FileSuggestionType::kLocalFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kLocalFile, path3,
           /*new_prediction_reason=*/absl::nullopt,
           /*new_score=*/absl::nullopt},
          {FileSuggestionType::kLocalFile, path4,
           /*new_prediction_reason=*/absl::nullopt,
           /*new_score=*/absl::nullopt}});
  suggestions = GetSuggestionsForType(FileSuggestionType::kLocalFile);
  EXPECT_EQ(2u, suggestions->size());
  EXPECT_EQ("file3", suggestions->at(0).file_path.value());
  EXPECT_EQ("zero_state_file://file3", suggestions->at(0).id);
  EXPECT_EQ("file4", suggestions->at(1).file_path.value());
  EXPECT_EQ("zero_state_file://file4", suggestions->at(1).id);

  file_suggest_service_->RemoveFileSuggestion(
      /*type=*/FileSuggestionType::kLocalFile,
      /*suggestion_id=*/"zero_state_file://" + path3.value());
  suggestions = GetSuggestionsForType(FileSuggestionType::kLocalFile);
  EXPECT_EQ(1u, suggestions->size());
  EXPECT_EQ("file4", suggestions->at(0).file_path.value());
  EXPECT_EQ("zero_state_file://file4", suggestions->at(0).id);

  file_suggest_service_->RemoveFileSuggestion(
      /*type=*/FileSuggestionType::kDriveFile,
      /*suggestion_id=*/"zero_state_drive://" + path2.value());
  suggestions = GetSuggestionsForType(FileSuggestionType::kDriveFile);
  EXPECT_TRUE(suggestions->empty());
}

}  // namespace app_list
