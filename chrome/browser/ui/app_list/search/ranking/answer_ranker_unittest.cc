// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/answer_ranker.h"

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

class TestAnswerResult : public ChromeSearchResult {
 public:
  explicit TestAnswerResult(double relevance) {
    SetDisplayType(DisplayType::kAnswerCard);
    set_relevance(relevance);
  }
  ~TestAnswerResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
};

Results make_answers(std::vector<double> relevances) {
  Results results;
  for (const double relevance : relevances) {
    results.push_back(std::make_unique<TestAnswerResult>(relevance));
  }
  return results;
}

}  // namespace

// Tests that all but one answer is filtered out.
TEST(AnswerRankerTest, FilterUnsuccessfulCandidates) {
  ResultsMap results_map;
  results_map[ResultType::kOmnibox] = make_answers({0.3, 0.5, 0.4});

  AnswerRanker ranker;
  ranker.UpdateResultRanks(results_map, ProviderType::kOmnibox);

  // Results should still be in the same order, with all except the highest
  // scoring answer filtered out.
  const auto& results = results_map[ResultType::kOmnibox];
  ASSERT_EQ(results.size(), 3);

  EXPECT_TRUE(results[0]->scoring().filter);
  EXPECT_FALSE(results[1]->scoring().filter);
  EXPECT_TRUE(results[2]->scoring().filter);
}

}  // namespace app_list
