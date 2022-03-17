// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/test/ranking_test_util.h"

namespace app_list {

// TestResult ------------------------------------------------------------------

TestResult::TestResult(const std::string& id,
                       double score,
                       ResultType result_type,
                       Category category) {
  set_id(id);
  SetDisplayScore(score);
  scoring().normalized_relevance = score;
  SetResultType(result_type);
  SetCategory(category);
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
    res.push_back(
        std::make_unique<TestResult>(ids[i], scores[i], result_type, category));
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
