// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/tools/filter_tool.h"
#include "components/subresource_filter/tools/indexing_tool.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace subresource_filter {

namespace {

static constexpr char kMetricIndexAndWriteTimeUs[] = "index_and_write_time";
static constexpr char kMetricMedianMatchTimeUs[] = "median_match_time";

}  // namespace

class IndexedRulesetPerftest : public testing::Test {
 public:
  IndexedRulesetPerftest() = default;

  IndexedRulesetPerftest(const IndexedRulesetPerftest&) = delete;
  IndexedRulesetPerftest& operator=(const IndexedRulesetPerftest&) = delete;

  ~IndexedRulesetPerftest() override = default;

  void SetUp() override {
    base::FilePath dir_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir_path);

    // The file contains the subresource URLs of the top-100 Alexa landing
    // pages.
    base::FilePath request_path = dir_path.AppendASCII(
        "components/subresource_filter/core/common/perftests/"
        "/data/http_archive_top_100_page_requests");
    base::ReadFileToString(request_path, &requests_);

    unindexed_path_ = dir_path.AppendASCII(
        "third_party/subresource-filter-ruleset/data/UnindexedRules");

    ASSERT_TRUE(scoped_dir_.CreateUniqueTempDir());
    base::FilePath indexed_path =
        scoped_dir_.GetPath().AppendASCII("IndexedRuleset");
    ASSERT_TRUE(base::PathExists(unindexed_path_));
    ASSERT_TRUE(base::DirectoryExists(indexed_path.DirName()));
    ASSERT_TRUE(IndexAndWriteRuleset(unindexed_path_, indexed_path));

    base::File indexed_file =
        base::File(indexed_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(indexed_file.IsValid());
    auto ruleset = subresource_filter::MemoryMappedRuleset::CreateAndInitialize(
        std::move(indexed_file));
    filter_tool_ = std::make_unique<FilterTool>(std::move(ruleset), &output_);
  }

  FilterTool* filter_tool() { return filter_tool_.get(); }

  const std::string& requests() const { return requests_; }

  const base::FilePath& unindexed_path() const { return unindexed_path_; }

  perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
    perf_test::PerfResultReporter reporter("IndexedRuleset.", story_name);
    reporter.RegisterImportantMetric(kMetricIndexAndWriteTimeUs, "us");
    reporter.RegisterImportantMetric(kMetricMedianMatchTimeUs, "us");
    return reporter;
  }

 private:
  base::ScopedTempDir scoped_dir_;
  base::FilePath unindexed_path_;

  std::string requests_;

  // Use an unopened output stream as a sort of null stream. All writes will
  // fail so things should be a bit faster than writing to a string.
  std::ofstream output_;

  std::unique_ptr<FilterTool> filter_tool_;
};

TEST_F(IndexedRulesetPerftest, IndexRuleset) {
  base::ScopedTempDir scoped_dir;
  ASSERT_TRUE(scoped_dir.CreateUniqueTempDir());
  base::FilePath indexed_path =
      scoped_dir.GetPath().AppendASCII("IndexedRuleset");

  base::ElapsedTimer timer;
  ASSERT_TRUE(IndexAndWriteRuleset(unindexed_path(), indexed_path));
  perf_test::PerfResultReporter reporter = SetUpReporter("IndexRuleset");
  reporter.AddResult(kMetricIndexAndWriteTimeUs,
                     static_cast<size_t>(timer.Elapsed().InMicroseconds()));
}

TEST_F(IndexedRulesetPerftest, MatchAll) {
  std::vector<int64_t> results;
  for (int i = 0; i < 5; ++i) {
    base::ElapsedTimer timer;
    std::istringstream request_stream(requests());
    filter_tool()->MatchBatch(&request_stream);
    results.push_back(timer.Elapsed().InMicroseconds());
  }
  std::sort(results.begin(), results.end());
  perf_test::PerfResultReporter reporter = SetUpReporter("MatchAll");
  reporter.AddResult(kMetricMedianMatchTimeUs, static_cast<size_t>(results[2]));
}

}  // namespace subresource_filter
