// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/test/ranking_test_util.h"

#include "base/strings/utf_string_conversions.h"

namespace app_list {

// TestResult ------------------------------------------------------------------

TestResult::TestResult(const std::string& id,
                       ResultType result_type,
                       Category category,
                       double display_score,
                       double normalized_relevance) {
  set_id(id);
  SetTitle(base::UTF8ToUTF16(id));
  SetResultType(result_type);
  SetCategory(category);
  SetDisplayScore(display_score);
  scoring().normalized_relevance = normalized_relevance;
}

TestResult::TestResult(const std::string& id,
                       double relevance,
                       double normalized_relevance,
                       DisplayType display_type,
                       bool best_match) {
  set_id(id);
  SetTitle(base::UTF8ToUTF16(id));
  set_relevance(relevance);
  scoring().normalized_relevance = normalized_relevance;
  SetDisplayType(display_type);
  SetBestMatch(best_match);
}

TestResult::TestResult(const std::string& id,
                       DisplayType display_type,
                       Category category,
                       int best_match_rank,
                       double relevance,
                       double ftrl_result_score) {
  set_id(id);
  SetTitle(base::UTF8ToUTF16(id));
  SetDisplayType(display_type);
  SetCategory(category);
  scoring().best_match_rank = best_match_rank;
  set_relevance(relevance);
  scoring().ftrl_result_score = ftrl_result_score;
}

TestResult::~TestResult() {}

// RankerTestBase --------------------------------------------------------------

void RankerTestBase::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}

base::FilePath RankerTestBase::GetPath() {
  return temp_dir_.GetPath().Append("proto");
}

Results RankerTestBase::MakeScoredResults(const std::vector<std::string>& ids,
                                          const std::vector<double> scores,
                                          ResultType result_type,
                                          Category category) {
  Results res;
  CHECK_EQ(ids.size(), scores.size());
  for (size_t i = 0; i < ids.size(); ++i) {
    res.push_back(std::make_unique<TestResult>(
        ids[i], result_type, category, /*display_score=*/scores[i],
        /*normalized_relevance=*/scores[i]));
  }
  return res;
}

Results RankerTestBase::MakeResults(const std::vector<std::string>& ids,
                                    ResultType result_type,
                                    Category category) {
  return RankerTestBase::MakeScoredResults(ids, std::vector<double>(ids.size()),
                                           result_type, category);
}

LaunchData RankerTestBase::MakeLaunchData(const std::string& id,
                                          ResultType result_type) {
  LaunchData launch;
  launch.launched_from = ash::AppListLaunchedFrom::kLaunchedFromSearchBox;
  launch.id = id;
  launch.result_type = result_type;
  return launch;
}

void RankerTestBase::Wait() {
  task_environment_.RunUntilIdle();
}

}  // namespace app_list
