// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"
#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class FileSuggestKeyedServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    file_suggest_service_ =
        std::make_unique<FileSuggestKeyedService>(profile_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<FileSuggestKeyedService> file_suggest_service_;
};

TEST_F(FileSuggestKeyedServiceTest, GetSuggestData) {
  base::HistogramTester tester;
  file_suggest_service_->GetSuggestFileData(
      FileSuggestKeyedService::SuggestionType::kItemSuggest,
      base::BindOnce(
          [](absl::optional<std::vector<FileSuggestData>> suggest_data) {
            EXPECT_FALSE(suggest_data.has_value());
          }));
  tester.ExpectBucketCount(
      "Ash.Search.DriveFileSuggestDataValidation.Status",
      /*sample=*/DriveSuggestValidationStatus::kDriveFSNotMounted,
      /*expected_count=*/1);
}

}  // namespace app_list
