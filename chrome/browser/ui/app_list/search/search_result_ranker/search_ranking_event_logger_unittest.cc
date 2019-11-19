// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/search_ranking_event_logger.h"

#include <map>
#include <memory>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history/core/test/test_history_database.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

namespace {

using ukm::TestUkmRecorder;

using ResultType = ash::AppListSearchResultType;
using UkmEntry = ukm::builders::AppListNonAppImpression;

std::unique_ptr<KeyedService> BuildHistoryService(
    content::BrowserContext* context) {
  TestingProfile* profile = static_cast<TestingProfile*>(context);

  base::FilePath history_path(profile->GetPath().Append("history"));

  // Delete the file before creating the service.
  if (!base::DeleteFile(history_path, false) ||
      base::PathExists(history_path)) {
    ADD_FAILURE() << "failed to delete history db file "
                  << history_path.value();
    return nullptr;
  }

  std::unique_ptr<history::HistoryService> history_service =
      std::make_unique<history::HistoryService>();
  if (history_service->Init(
          history::TestHistoryDatabaseParamsForPath(profile->GetPath()))) {
    return std::move(history_service);
  }

  ADD_FAILURE() << "failed to initialize history service";
  return nullptr;
}

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id,
                   ResultType type,
                   int subtype,
                   double relevance)
      : instance_id_(instantiation_count++) {
    set_id(id);
    set_result_subtype(subtype);
    set_relevance(relevance);
    SetTitle(base::UTF8ToUTF16(id));
    SetResultType(type);
  }
  ~TestSearchResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
  void InvokeAction(int action_index, int event_flags) override {}
  ash::SearchResultType GetSearchResultType() const override {
    return ash::SEARCH_RESULT_TYPE_BOUNDARY;
  }

 private:
  static int instantiation_count;

  int instance_id_;
};
int TestSearchResult::instantiation_count = 0;

class SearchControllerFake : public SearchController {
 public:
  explicit SearchControllerFake(Profile* profile)
      : SearchController(nullptr, nullptr, profile) {}

  ChromeSearchResult* FindSearchResult(const std::string& result_id) override {
    auto it = results_.find(result_id);
    CHECK(it != results_.end());
    return &it->second;
  }

  void AddSearchResult(const std::string& result_id,
                       ResultType type,
                       int subtype,
                       double relevance) {
    results_.emplace(
        std::piecewise_construct, std::forward_as_tuple(result_id),
        std::forward_as_tuple(result_id, type, subtype, relevance));
  }

  std::map<std::string, TestSearchResult> results_;
};

}  // namespace

class SearchRankingEventLoggerTest : public testing::Test {
 public:
  SearchRankingEventLoggerTest() {}

  void SetUp() override {
    ASSERT_TRUE(history_dir_.CreateUniqueTempDir());
    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(history_dir_.GetPath()));

    TestingProfile::TestingFactories factories;

    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("testuser@gmail.com");
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        base::BindRepeating(&BuildHistoryService));
    profile_ = profile_builder.Build();

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {}, {app_list_features::kEnableQueryBasedMixedTypesRanker,
             app_list_features::kEnableAppRanker});

    search_controller_ = std::make_unique<SearchControllerFake>(profile_.get());

    logger_ = std::make_unique<SearchRankingEventLogger>(
        profile_.get(), search_controller_.get());
  }

  void AddResultWithHistory(const std::string& result_id,
                            ResultType type,
                            int subtype,
                            double relevance) {
    search_controller_->AddSearchResult(result_id, type, subtype, relevance);
    history_service_->AddPage(GURL(result_id), base::Time::Now(),
                              history::VisitSource::SOURCE_BROWSED);
    Wait();
  }

  void AddResultWithoutHistory(const std::string& result_id,
                               ResultType type,
                               int subtype,
                               double relevance) {
    search_controller_->AddSearchResult(result_id, type, subtype, relevance);
  }

  std::vector<const ukm::mojom::UkmEntry*> GetUkmEntries() {
    return recorder_.GetEntriesByName(UkmEntry::kEntryName);
  }

  void SetLocalTime(const int day, const int hour, const int minute = 10) {
    // Advance time to 00:00 on the next local Sunday.
    base::Time::Exploded now;
    base::Time::Now().LocalExplode(&now);
    const auto sunday = base::TimeDelta::FromDays(6 - now.day_of_week) +
                        base::TimeDelta::FromHours(23 - now.hour) +
                        base::TimeDelta::FromMinutes(60 - now.minute);
    if (sunday > base::TimeDelta())
      time_.Advance(sunday);

    base::Time::Now().LocalExplode(&now);
    CHECK_EQ(now.day_of_week, 0);
    CHECK_EQ(now.hour, 0);
    CHECK_EQ(now.minute, 0);

    // Then advance to the given time and day.
    const auto advance = base::TimeDelta::FromDays(day) +
                         base::TimeDelta::FromHours(hour) +
                         base::TimeDelta::FromMinutes(minute);
    if (advance > base::TimeDelta())
      time_.Advance(advance);
  }

  // Wait for separate background task runner in HistoryService to complete
  // all tasks and then all the tasks on the current one to complete as well.
  void Wait() {
    history::BlockUntilHistoryProcessesPendingRequests(history_service_.get());
    task_environment_.RunUntilIdle();
  }

  // Wait for a background UKM event to finish processing. This is necessary if
  // the logger needs to query UkmBackgroundRecorderService for a Source ID,
  // which currently happens if the search result is from the Omnibox.
  //
  // Warning: this will fail if more than one search result exists within one
  // call to SearchRankingEventLogger::Log.
  void WaitForUkmEvent() {
    base::RunLoop run_loop;
    logger_->SetEventRecordedForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedMockClockOverride time_;
  base::ScopedTempDir history_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;

  ukm::TestAutoSetUkmRecorder recorder_;

  std::unique_ptr<Profile> profile_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<SearchControllerFake> search_controller_;
  std::unique_ptr<SearchRankingEventLogger> logger_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchRankingEventLoggerTest);
};

// TODO(931149): We should add a test to ensure that a result with a URL in the
// history service has the correct source ID.

// Test a single logging call containing only one result. As there is only one
// event, we don't test any of the stateful fields that rely on previous
// launches.
TEST_F(SearchRankingEventLoggerTest, OneEventWithOneResult) {
  SetLocalTime(5, 13, 25);

  AddResultWithoutHistory("myfile.txt", ResultType::kLauncher, 2, 0.18);

  logger_->Log(base::UTF8ToUTF16("elevenchars"), {{"myfile.txt", 2}}, 1);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(entries.size(), 1ul);

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "EventId", 1);
  TestUkmRecorder::ExpectEntryMetric(entry, "Position", 2);
  TestUkmRecorder::ExpectEntryMetric(entry, "IsLaunched", 0);
  TestUkmRecorder::ExpectEntryMetric(entry, "QueryLength", 11);
  TestUkmRecorder::ExpectEntryMetric(entry, "RelevanceScore", 18);
  TestUkmRecorder::ExpectEntryMetric(entry, "Category", 1);
  TestUkmRecorder::ExpectEntryMetric(entry, "FileExtension", 57);
  TestUkmRecorder::ExpectEntryMetric(entry, "HourOfDay", 13);
  TestUkmRecorder::ExpectEntryMetric(entry, "DayOfWeek", 5);
}

// Test a single logging call containing three results.
TEST_F(SearchRankingEventLoggerTest, OneEventWithManyResults) {
  SetLocalTime(4, 16, 10);

  // Subtype is unused here.
  AddResultWithoutHistory("first.avi", ResultType::kLauncher, 0, 0.7);
  AddResultWithoutHistory("second.mkv", ResultType::kLauncher, 0, 0.6);
  AddResultWithoutHistory("third.mov", ResultType::kLauncher, 0, 0.5);

  logger_->Log(base::UTF8ToUTF16("ninechars"),
               {{"first.avi", 0}, {"second.mkv", 1}, {"third.mov", 2}}, 1);

  const auto& entries = GetUkmEntries();
  ASSERT_EQ(entries.size(), 3ul);

  const auto* entry = entries[0];
  TestUkmRecorder::ExpectEntryMetric(entry, "EventId", 1);
  TestUkmRecorder::ExpectEntryMetric(entry, "Position", 0);
  TestUkmRecorder::ExpectEntryMetric(entry, "IsLaunched", 0);
  TestUkmRecorder::ExpectEntryMetric(entry, "QueryLength", 9);
  TestUkmRecorder::ExpectEntryMetric(entry, "RelevanceScore", 70);
  TestUkmRecorder::ExpectEntryMetric(entry, "Category", 1);
  TestUkmRecorder::ExpectEntryMetric(entry, "FileExtension", 5);
  TestUkmRecorder::ExpectEntryMetric(entry, "HourOfDay", 16);
  TestUkmRecorder::ExpectEntryMetric(entry, "DayOfWeek", 4);

  entry = entries[1];
  TestUkmRecorder::ExpectEntryMetric(entry, "EventId", 1);
  TestUkmRecorder::ExpectEntryMetric(entry, "Position", 1);
  TestUkmRecorder::ExpectEntryMetric(entry, "IsLaunched", 1);
  TestUkmRecorder::ExpectEntryMetric(entry, "QueryLength", 9);
  TestUkmRecorder::ExpectEntryMetric(entry, "RelevanceScore", 60);
  TestUkmRecorder::ExpectEntryMetric(entry, "Category", 1);
  TestUkmRecorder::ExpectEntryMetric(entry, "FileExtension", 20);
  TestUkmRecorder::ExpectEntryMetric(entry, "HourOfDay", 16);
  TestUkmRecorder::ExpectEntryMetric(entry, "DayOfWeek", 4);

  entry = entries[2];
  TestUkmRecorder::ExpectEntryMetric(entry, "EventId", 1);
  TestUkmRecorder::ExpectEntryMetric(entry, "Position", 2);
  TestUkmRecorder::ExpectEntryMetric(entry, "IsLaunched", 0);
  TestUkmRecorder::ExpectEntryMetric(entry, "QueryLength", 9);
  TestUkmRecorder::ExpectEntryMetric(entry, "RelevanceScore", 50);
  TestUkmRecorder::ExpectEntryMetric(entry, "Category", 1);
  TestUkmRecorder::ExpectEntryMetric(entry, "FileExtension", 21);
  TestUkmRecorder::ExpectEntryMetric(entry, "HourOfDay", 16);
  TestUkmRecorder::ExpectEntryMetric(entry, "DayOfWeek", 4);
}

// Tests the fields that are affected by previous launches of a given search
// result.
TEST_F(SearchRankingEventLoggerTest, StatefulFieldsOfRepeatedEvents) {
  SetLocalTime(4, 16, 10);
  AddResultWithoutHistory("file.txt", ResultType::kLauncher, 3, 0.7);

  logger_->Log(base::UTF8ToUTF16("query"), {{"file.txt", 0}}, 0);

  {
    const auto& entries = GetUkmEntries();
    ASSERT_EQ(entries.size(), 1ul);
    TestUkmRecorder::ExpectEntryMetric(entries[0], "EventId", 1);
    TestUkmRecorder::ExpectEntryMetric(entries[0], "LaunchesThisSession", 0);
  }

  // Record four more evenets, but only three of them are launches (two for the
  // purposes of the metrics for the last event).
  logger_->Log(base::UTF8ToUTF16("query"), {{"file.txt", 1}}, 1);
  // Not a launch:
  logger_->Log(base::UTF8ToUTF16("abcd"), {{"file.txt", 0}}, -1);
  logger_->Log(base::UTF8ToUTF16("abcd"), {{"file.txt", 2}}, 2);
  logger_->Log(base::UTF8ToUTF16(""), {{"file.txt", 0}}, 0);

  {
    const auto& entries = GetUkmEntries();
    ASSERT_EQ(entries.size(), 5ul);
    TestUkmRecorder::ExpectEntryMetric(entries[4], "EventId", 5);
    TestUkmRecorder::ExpectEntryMetric(entries[4], "LaunchesThisSession", 3);
    TestUkmRecorder::ExpectEntryMetric(entries[4], "TimeSinceLastLaunch", 0);
    TestUkmRecorder::ExpectEntryMetric(entries[4], "TimeOfLastLaunch", 16);
    TestUkmRecorder::ExpectEntryMetric(entries[4], "LaunchesAtHour16", 3);
  }

  // Advance time by 24 hours. We expect LaunchesAtHour16 to be reset to 0.
  time_.Advance(base::TimeDelta::FromHours(24));

  logger_->Log(base::UTF8ToUTF16(""), {{"file.txt", 0}}, 0);

  {
    const auto& entries = GetUkmEntries();
    ASSERT_EQ(entries.size(), 6ul);
    // Bin of 24*60*60 has value 85507, which is the 211th bin overall.
    TestUkmRecorder::ExpectEntryMetric(entries[5], "TimeSinceLastLaunch",
                                       85507);
    TestUkmRecorder::ExpectEntryMetric(entries[5], "TimeOfLastLaunch", 16);
    TestUkmRecorder::ExpectEntryMetric(entries[5], "LaunchesAtHour16", 0);
  }
}

}  // namespace app_list
