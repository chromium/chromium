// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/chip_ranker.h"

#include <list>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::UnorderedElementsAre;
using testing::WhenSorted;

namespace app_list {
namespace {

using ResultType = ash::AppListSearchResultType;

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id, ResultType type)
      : instance_id_(instantiation_count++) {
    set_id(id);
    SetTitle(base::UTF8ToUTF16(id));
    SetResultType(type);

    switch (type) {
      case ResultType::kFileChip:
      case ResultType::kDriveChip:
        SetDisplayType(DisplayType::kChip);
        break;
      case ResultType::kInstalledApp:
        // Apps that should be in the chips
        SetDisplayType(DisplayType::kTile);
        SetIsRecommendation(true);
        break;
      case ResultType::kPlayStoreApp:
        // Apps that shouldn't be in the chips
        SetDisplayType(DisplayType::kTile);
        break;
      default:
        SetDisplayType(DisplayType::kList);
        break;
    }
  }
  ~TestSearchResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}

 private:
  static int instantiation_count;

  int instance_id_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchResult);
};

int TestSearchResult::instantiation_count = 0;

MATCHER_P(HasId, id, "") {
  bool match = base::UTF16ToUTF8(arg.result->title()) == id;
  if (!match)
    *result_listener << "HasId wants '" << id << "', but got '"
                     << arg.result->title() << "'";
  return match;
}

MATCHER_P(HasScore, score, "") {
  const double tol = 1e-10;
  bool match = abs(arg.score - score) < tol;
  if (!match)
    *result_listener << "HasScore wants '" << score << "', but got '"
                     << arg.score << "'";
  return match;
}

}  // namespace

class ChipRankerTest : public testing::Test {
 public:
  ChipRankerTest() {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();

    ranker_ = std::make_unique<ChipRanker>(profile_.get());
    task_environment_.RunUntilIdle();
  }

  ~ChipRankerTest() override = default;

  Mixer::SortedResults MakeSearchResults(const std::vector<std::string>& ids,
                                         const std::vector<ResultType>& types,
                                         const std::vector<double> scores) {
    Mixer::SortedResults results;
    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
      results_.emplace_back(ids[i], types[i]);
      results.emplace_back(&results_.back(), scores[i]);
    }
    return results;
  }

  void TrainRanker(const std::vector<std::string>& types) {
    // Clear the ranker of existing scores.
    auto* type_ranker = ranker_->GetRankerForTest();
    type_ranker->GetTargetData()->clear();

    for (const std::string& type : types) {
      type_ranker->Record(type);
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<Profile> profile_;

  std::unique_ptr<ChipRanker> ranker_;

  // This is used only to make the ownership clear for the TestSearchResult
  // objects that the return value of MakeSearchResults() contains raw pointers
  // to.
  std::list<TestSearchResult> results_;
};

// Check that ranking an empty list has no effect.
TEST_F(ChipRankerTest, EmptyList) {
  Mixer::SortedResults results = MakeSearchResults({}, {}, {});
  ranker_->Rank(&results);
  EXPECT_EQ(results.size(), 0ul);
}

// Check that ranking only apps has no effect.
TEST_F(ChipRankerTest, AppsOnly) {
  Mixer::SortedResults results =
      MakeSearchResults({"app1", "app2", "app3"},
                        {ResultType::kInstalledApp, ResultType::kPlayStoreApp,
                         ResultType::kInstalledApp},
                        {8.9, 8.8, 8.7});

  TrainRanker({"app", "file"});

  ranker_->Rank(&results);
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("app2"),
                                              HasId("app3"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.8),
                                              HasScore(8.7))));
}

// Check that ranking a non-chip result does not affect its score.
TEST_F(ChipRankerTest, UnchangedItem) {
  Mixer::SortedResults results =
      MakeSearchResults({"app1", "app2", "omni1", "omni2"},
                        {ResultType::kInstalledApp, ResultType::kInstalledApp,
                         ResultType::kOmnibox, ResultType::kOmnibox},
                        {8.9, 8.7, 0.8, 0.7});

  TrainRanker({"app", "file"});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("app2"),
                                              HasId("omni1"), HasId("omni2"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.7),
                                              HasScore(0.8), HasScore(0.7))));
}

// With no training, we expect the results list to be: app, app, file, app,
// file. Note this might be different from what is actually seen on devices,
// depending on whether apps initially have identical scores.
TEST_F(ChipRankerTest, DefaultInitialization) {
  Mixer::SortedResults results = MakeSearchResults(
      {"app1", "app2", "app3", "drive1", "drive2", "local1", "local2"},
      {ResultType::kInstalledApp, ResultType::kInstalledApp,
       ResultType::kInstalledApp, ResultType::kDriveChip,
       ResultType::kDriveChip, ResultType::kFileChip, ResultType::kFileChip},
      {8.9, 8.7, 8.5, 0.9, 0.7, 0.8, 0.6});
  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("app2"),
                                              HasId("drive1"), HasId("drive2"),
                                              HasId("local1"), HasId("app3"),
                                              HasId("local2"))));
}

}  // namespace app_list
