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

class TestResult : public ChromeSearchResult {
 public:
  TestResult(double relevance, DisplayType display_type, bool best_match) {
    set_relevance(relevance);
    SetDisplayType(display_type);
    SetBestMatch(best_match);
  }
  ~TestResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
};

Results make_omnibox_candidates(std::vector<double> relevances) {
  Results results;
  for (const double relevance : relevances) {
    results.push_back(std::make_unique<TestResult>(
        relevance, ash::SearchResultDisplayType::kAnswerCard, false));
  }
  return results;
}

Results make_shortcut_candidates(std::vector<bool> best_matches) {
  Results results;
  for (const double best_match : best_matches) {
    results.push_back(std::make_unique<TestResult>(
        1, ash::SearchResultDisplayType::kList, best_match));
  }
  return results;
}

}  // namespace

// Tests that the best Omnibox answer is selected and all others are filtered
// out.
TEST(AnswerRankerTest, SelectAndFilterOmnibox) {
  ResultsMap results_map;
  results_map[ResultType::kOmnibox] = make_omnibox_candidates({0.3, 0.5, 0.4});

  AnswerRanker ranker;
  ranker.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ranker.OnBurnInPeriodElapsed();

  const auto& results = results_map[ResultType::kOmnibox];
  ASSERT_EQ(results.size(), 3u);

  // The highest scoring Omnibox answer is selected.
  EXPECT_EQ(results[1]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_FALSE(results[1]->scoring().filter);

  // Others are filtered out.
  EXPECT_TRUE(results[0]->scoring().filter);
  EXPECT_TRUE(results[2]->scoring().filter);
}

// Tests that a best match shortcut is selected.
TEST(AnswerRankerTest, SelectBestShortcut) {
  ResultsMap results_map;
  results_map[ResultType::kKeyboardShortcut] =
      make_shortcut_candidates({false, true});

  AnswerRanker ranker;
  ranker.UpdateResultRanks(results_map, ProviderType::kKeyboardShortcut);
  ranker.OnBurnInPeriodElapsed();

  const auto& results = results_map[ResultType::kKeyboardShortcut];
  ASSERT_EQ(results.size(), 2u);

  // The best match shortcut is selected.
  EXPECT_EQ(results[1]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_FALSE(results[1]->scoring().filter);

  EXPECT_NE(results[0]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);
}

// Tests that no shortcut answers are selected if there are multiple best
// matches.
TEST(AnswerRankerTest, OnlySelectIfOneBestShortcut) {
  ResultsMap results_map;
  results_map[ResultType::kKeyboardShortcut] =
      make_shortcut_candidates({true, true});

  AnswerRanker ranker;
  ranker.UpdateResultRanks(results_map, ProviderType::kKeyboardShortcut);
  ranker.OnBurnInPeriodElapsed();

  const auto& results = results_map[ResultType::kKeyboardShortcut];
  ASSERT_EQ(results.size(), 2u);

  // No shortcuts should be selected.
  for (const auto& result : results) {
    EXPECT_NE(result->display_type(),
              ash::SearchResultDisplayType::kAnswerCard);
  }
}

// Tests that Omnibox answers take priority over Shortcuts.
TEST(AnswerRankerTest, OmniboxOverShortcuts) {
  ResultsMap results_map;
  results_map[ResultType::kOmnibox] = make_omnibox_candidates({0.4});
  results_map[ResultType::kKeyboardShortcut] = make_shortcut_candidates({true});

  AnswerRanker ranker;
  ranker.UpdateResultRanks(results_map, ProviderType::kKeyboardShortcut);
  ranker.UpdateResultRanks(results_map, ProviderType::kOmnibox);
  ranker.OnBurnInPeriodElapsed();

  // Shortcut candidate should not be selected.
  const auto& shortcut_results = results_map[ResultType::kKeyboardShortcut];
  ASSERT_EQ(shortcut_results.size(), 1u);
  EXPECT_NE(shortcut_results[0]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);

  // Omnibox candidate should be selected.
  const auto& omnibox_results = results_map[ResultType::kOmnibox];
  ASSERT_EQ(omnibox_results.size(), 1u);
  EXPECT_EQ(omnibox_results[0]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_FALSE(omnibox_results[0]->scoring().filter);
}

// Tests that a chosen answer is not changed after burn-in.
TEST(AnswerRankerTest, SelectedAnswerNotChangedAfterBurnIn) {
  ResultsMap results_map;
  results_map[ResultType::kKeyboardShortcut] = make_shortcut_candidates({true});

  AnswerRanker ranker;
  ranker.UpdateResultRanks(results_map, ProviderType::kKeyboardShortcut);
  ranker.OnBurnInPeriodElapsed();

  // The shortcut answer is selected.
  const auto& shortcut_results = results_map[ResultType::kKeyboardShortcut];
  ASSERT_EQ(shortcut_results.size(), 1u);
  EXPECT_EQ(shortcut_results[0]->display_type(),
            ash::SearchResultDisplayType::kAnswerCard);

  // New Omnibox candidates should still be filtered out.
  results_map[ResultType::kOmnibox] = make_omnibox_candidates({0.5});
  ranker.UpdateResultRanks(results_map, ProviderType::kOmnibox);

  const auto& omnibox_results = results_map[ResultType::kOmnibox];
  ASSERT_EQ(omnibox_results.size(), 1u);
  EXPECT_TRUE(omnibox_results[0]->scoring().filter);
}

}  // namespace app_list
