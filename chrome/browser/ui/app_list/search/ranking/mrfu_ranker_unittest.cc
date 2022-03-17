// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/mrfu_ranker.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/test/ranking_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using testing::ElementsAre;

}  // namespace

// MrfuResultRanker ------------------------------------------------------------

class MrfuResultRankerTest : public RankerTestBase {};

TEST_F(MrfuResultRankerTest, TrainAndRank) {
  MrfuResultRanker ranker(MrfuCache::Params(),
                          MrfuCache::Proto(GetPath(), base::Seconds(0)));
  Wait();

  // Train on some results.
  ranker.Train(MakeLaunchData("a"));
  ranker.Train(MakeLaunchData("b"));
  ranker.Train(MakeLaunchData("c"));
  ranker.Train(MakeLaunchData("a"));

  ResultsMap results;
  results[ResultType::kInstalledApp] = MakeResults({"a", "b"});
  results[ResultType::kOsSettings] = MakeResults({"c", "d"});
  CategoriesList categories;

  // Rank them, expecting ordering a > c > b > d.
  ranker.Start(u"query", results, categories);
  auto ab_scores = ranker.GetResultRanks(results, ResultType::kInstalledApp);
  auto cd_scores = ranker.GetResultRanks(results, ResultType::kOsSettings);
  EXPECT_GT(ab_scores[0], cd_scores[0]);
  EXPECT_GT(cd_scores[0], ab_scores[1]);
  EXPECT_GT(ab_scores[1], cd_scores[1]);
}

// MrfuCategoryRanker ----------------------------------------------------------

class MrfuCategoryRankerTest : public RankerTestBase {};

TEST_F(MrfuCategoryRankerTest, TrainAndRank) {
  MrfuCategoryRanker ranker(MrfuCache::Params(),
                            MrfuCache::Proto(GetPath(), base::Seconds(0)));
  Wait();

  // Train so that settings should be first, followed by apps.
  ranker.Train(MakeLaunchData("a", ResultType::kInstalledApp));
  ranker.Train(MakeLaunchData("c", ResultType::kOsSettings));
  ranker.Train(MakeLaunchData("d", ResultType::kOsSettings));
  ranker.Train(MakeLaunchData("b", ResultType::kInstalledApp));

  ResultsMap results;
  results[ResultType::kInstalledApp] =
      MakeResults({"a", "b"}, ResultType::kInstalledApp, Category::kApps);
  results[ResultType::kOsSettings] =
      MakeResults({"c", "d"}, ResultType::kOsSettings, Category::kSettings);
  results[ResultType::kFileSearch] =
      MakeResults({"e", "f"}, ResultType::kFileSearch, Category::kFiles);
  CategoriesList categories({{.category = Category::kApps},
                             {.category = Category::kSettings},
                             {.category = Category::kFiles}});

  // Expect a ranking of kInstalledApp > kOsSettings > kFileSearch.
  ranker.Start(u"query", results, categories);
  auto scores =
      ranker.GetCategoryRanks(results, categories, ResultType::kInstalledApp);
  ASSERT_EQ(scores.size(), 3u);
  EXPECT_GT(scores[0], scores[1]);
  EXPECT_GT(scores[1], scores[2]);
}

}  // namespace app_list
