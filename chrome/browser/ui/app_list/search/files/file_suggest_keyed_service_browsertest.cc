// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "content/public/test/browser_test.h"

namespace app_list {
namespace {

class MockObserver : public FileSuggestKeyedService::Observer {
 public:
  explicit MockObserver(FileSuggestKeyedService* file_suggest_service)
      : file_suggest_service_(file_suggest_service) {
    file_suggest_service_observation_.Observe(file_suggest_service_);
  }
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  void WaitUntilFetchingSuggestData() { run_loop_.Run(); }

  // Returns the most recently fetched suggest data.
  const absl::optional<std::vector<FileSuggestData>>& last_fetched_data()
      const {
    return last_fetched_data_;
  }

 private:
  void OnSuggestFileDataFetched(
      absl::optional<std::vector<FileSuggestData>> suggest_data_array) {
    last_fetched_data_ = suggest_data_array;
    run_loop_.Quit();
  }

  // FileSuggestKeyedService::Observer:
  void OnFileSuggestionUpdated(
      FileSuggestKeyedService::SuggestionType type) override {
    EXPECT_EQ(FileSuggestKeyedService::SuggestionType::kItemSuggest, type);
    file_suggest_service_->GetSuggestFileData(
        FileSuggestKeyedService::SuggestionType::kItemSuggest,
        base::BindOnce(&MockObserver::OnSuggestFileDataFetched,
                       base::Unretained(this)));
  }

  FileSuggestKeyedService* const file_suggest_service_;
  base::RunLoop run_loop_;
  absl::optional<std::vector<FileSuggestData>> last_fetched_data_;
  base::ScopedObservation<FileSuggestKeyedService,
                          FileSuggestKeyedService::Observer>
      file_suggest_service_observation_{this};
};

/*
The suggest item metadata. It matches the json response used by
`ItemSuggestCache`. A sample json response is listed as below:
 R"(
    {
      "item": [
        {
          "itemId": "id",
          "displayText": "text",
          "predictionReason": "reason"
        }
      ],
      "suggestionSessionId": "session id"
    })";
*/
struct SuggestItemMetaData {
  std::string item_id;
  std::string display_text;
  std::string prediction_reason;
};

// Calculates a json string used to update the drive suggest cache.
std::string CalculateDriveSuggestUpdateJsonString(
    const std::vector<SuggestItemMetaData>& data_array,
    const std::string& session_id) {
  base::Value::List list_value;
  for (const auto& data : data_array) {
    base::Value::Dict dict_value;
    dict_value.Set("itemId", data.item_id);
    dict_value.Set("displayText", data.display_text);
    dict_value.Set("predictionReason", data.prediction_reason);
    list_value.Append(std::move(dict_value));
  }

  base::Value::Dict suggest_item_update;
  suggest_item_update.Set("item", std::move(list_value));
  suggest_item_update.Set("suggestionSessionId", session_id);

  std::string json_string;
  base::JSONWriter::Write(suggest_item_update, &json_string);
  return json_string;
}

}  // namespace

using FileSuggestKeyedServiceBrowserTest =
    drive::DriveIntegrationServiceBrowserTestBase;

// Verifies that the file suggest keyed service works as expected when the item
// suggest cache is empty.
IN_PROC_BROWSER_TEST_F(FileSuggestKeyedServiceBrowserTest,
                       QueryWithEmptySuggestCache) {
  base::HistogramTester tester;
  app_list::FileSuggestKeyedService* service =
      app_list::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          browser()->profile());
  service->GetSuggestFileData(
      FileSuggestKeyedService::SuggestionType::kItemSuggest,
      base::BindOnce(
          [](absl::optional<std::vector<FileSuggestData>> suggest_data) {
            EXPECT_FALSE(suggest_data.has_value());
          }));
  tester.ExpectBucketCount("Ash.Search.DriveFileSuggestDataValidation.Status",
                           /*sample=*/DriveSuggestValidationStatus::kNoResults,
                           /*expected_count=*/1);
}

// Verifies that the file suggest keyed service responds to the update in
// the item suggest cache correctly.
IN_PROC_BROWSER_TEST_F(FileSuggestKeyedServiceBrowserTest,
                       RespondToItemSuggestCacheUpdate) {
  Profile* profile = browser()->profile();
  InitTestFileMountRoot(profile);

  // Add two drive files.
  const std::string file_id1("abc123");
  base::FilePath absolute_file_path1;
  AddDriveFileWithRelativePath(profile, file_id1, base::FilePath(""),
                               /*new_file_relative_path=*/nullptr,
                               &absolute_file_path1);
  const std::string file_id2("qwertyqwerty");
  base::FilePath absolute_file_path2;
  AddDriveFileWithRelativePath(profile, file_id2, base::FilePath(""),
                               /*new_file_relative_path=*/nullptr,
                               &absolute_file_path2);

  // A file id that does not exist in the drive file system.
  const std::string non_existed_id("non_existed");

  app_list::FileSuggestKeyedService* service =
      app_list::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          profile);
  base::HistogramTester tester;

  // Verify in the scenario that all suggested file paths are invalid.
  {
    // Ensure that `observer` exists before updating the suggest cache. Because
    // notifying the observer of the suggest cache update is synchronous.
    MockObserver observer(service);

    // Update the item suggest cache with a non-existed file id.
    service->item_suggest_cache_for_test()->UpdateCacheWithJsonForTest(
        CalculateDriveSuggestUpdateJsonString(
            {{non_existed_id, "dispaly text 1", "prediction reason 1"}},
            "suggestion id 0"));

    observer.WaitUntilFetchingSuggestData();
    const auto& fetched_data = observer.last_fetched_data();
    EXPECT_FALSE(fetched_data.has_value());
    tester.ExpectBucketCount(
        "Ash.Search.DriveFileSuggestDataValidation.Status",
        /*sample=*/DriveSuggestValidationStatus::kAllFilesErrored,
        /*expected_count=*/1);
  }

  // Verify in the scenario that some suggested file paths are invalid.
  {
    MockObserver observer(service);

    // Update the item suggest cache with two file ids: one is valid and the
    // other is not.
    std::string json_string = CalculateDriveSuggestUpdateJsonString(
        {{"abc123", "dispaly text 1", "prediction reason 1"},
         {non_existed_id, "display text 2", "prediction reason 2"}},
        "suggestion id 1");
    service->item_suggest_cache_for_test()->UpdateCacheWithJsonForTest(
        json_string);

    observer.WaitUntilFetchingSuggestData();
    const auto& fetched_data = observer.last_fetched_data();
    EXPECT_TRUE(fetched_data.has_value());
    EXPECT_EQ(1u, fetched_data->size());
    EXPECT_EQ(absolute_file_path1, fetched_data->at(0).file_path);
    EXPECT_EQ("prediction reason 1", *fetched_data->at(0).prediction_reason);
    tester.ExpectBucketCount("Ash.Search.DriveFileSuggestDataValidation.Status",
                             /*sample=*/DriveSuggestValidationStatus::kOk,
                             /*expected_count=*/1);
  }

  // Verify in the scenario that all suggested file paths are valid.
  {
    MockObserver observer(service);

    // Update the item suggest cache with two valid ids.
    std::string json_string = CalculateDriveSuggestUpdateJsonString(
        {{"abc123", "dispaly text 1", "prediction reason 1"},
         {"qwertyqwerty", "display text 2", "prediction reason 2"}},
        "suggestion id 2");
    service->item_suggest_cache_for_test()->UpdateCacheWithJsonForTest(
        json_string);

    observer.WaitUntilFetchingSuggestData();
    tester.ExpectBucketCount("Ash.Search.DriveFileSuggestDataValidation.Status",
                             /*sample=*/DriveSuggestValidationStatus::kOk,
                             /*expected_count=*/2);

    // Verify the fetched data.
    const auto& fetched_data = observer.last_fetched_data();
    EXPECT_TRUE(fetched_data.has_value());
    EXPECT_EQ(2u, fetched_data->size());
    EXPECT_EQ(absolute_file_path1, fetched_data->at(0).file_path);
    EXPECT_EQ("prediction reason 1", *fetched_data->at(0).prediction_reason);
    EXPECT_EQ(absolute_file_path2, fetched_data->at(1).file_path);
    EXPECT_EQ("prediction reason 2", *fetched_data->at(1).prediction_reason);
  }
}

}  // namespace app_list
