// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_TEST_RANKING_TEST_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_TEST_RANKING_TEST_UTIL_H_

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class TestResult : public ChromeSearchResult {
 public:
  // TestResult is used by many test suites. Each test suite operates on
  // different members of ChromeSearchResult. This set of constructors aims to
  //
  // (a) generalize across the use cases (to minimize constructor number) and
  // (b) retain flexibility (to keep points-of-use from becoming cumbersome).
  TestResult() = default;

  TestResult(const std::string& id,
             ResultType result_type = ResultType::kUnknown,
             Category category = Category::kUnknown,
             double display_score = 0.0,
             double normalized_relevance = 0.0);

  TestResult(const std::string& id,
             double relevance,
             double normalized_relevance = 0.0,
             DisplayType display_type = DisplayType::kNone,
             bool best_match = false);

  TestResult(const std::string& id,
             Category category,
             int best_match_rank,
             double relevance,
             double ftrl_result_score);

  ~TestResult() override;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
};

class RankerTestBase : public testing::Test {
 public:
  void SetUp() override;

  base::FilePath GetPath();

  // A helper function for creating results. For convenience, the provided
  // scores are set as both the display score and normalized relevance.
  Results MakeScoredResults(const std::vector<std::string>& ids,
                            const std::vector<double> scores,
                            ResultType result_type = ResultType::kUnknown,
                            Category category = Category::kUnknown);

  // A helper function for creating results, for when results don't need scores.
  Results MakeResults(const std::vector<std::string>& ids,
                      ResultType result_type = ResultType::kUnknown,
                      Category category = Category::kUnknown);

  LaunchData MakeLaunchData(const std::string& id,
                            ResultType result_type = ResultType::kUnknown);

  template <typename T>
  T ReadProtoFromDisk() {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    T proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  void Wait();

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_TEST_RANKING_TEST_UTIL_H_
