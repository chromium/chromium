// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/indexed_ruleset.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/subresource_filter/core/common/copying_file_stream.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/common/unindexed_ruleset.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/subresource_filter/tools/filter_tool.h"
#include "components/subresource_filter/tools/indexing_tool.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

namespace {

namespace proto = url_pattern_index::proto;

static constexpr char kMetricIndexAndWriteTimeUs[] = "index_and_write_time";
static constexpr char kMetricIndexingTimeUs[] = "indexing_time";
static constexpr char kMetricMedianMatchTimeUs[] = "median_match_time";
static constexpr char kMetricStyleMatchTimeUs[] = "style_match_time";

}  // namespace

class IndexedRulesetPerftest : public ::testing::Test {
 public:
  static constexpr int kNumBenchmarkOrigins = 100;
  static constexpr int kNumLookupsPerOrigin = 100;

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
    indexed_path_ = scoped_dir_.GetPath().AppendASCII("IndexedRuleset");
    unindexed_with_style_path_ =
        scoped_dir_.GetPath().AppendASCII("IndexWithStyleRuleset");
    ASSERT_TRUE(base::PathExists(unindexed_path_));
    ASSERT_TRUE(base::DirectoryExists(indexed_path_.DirName()));

    // Generate 9,600 unique synthetic domains to match EasyList's real-world
    // density.
    std::vector<std::string> all_domains;
    for (int i = 0; i < 8000; ++i) {
      std::string domain = "example" + base::NumberToString(i) + ".com";
      all_domains.push_back("https://" + domain);
      if (i % 5 == 0) {
        all_domains.push_back("https://sub.domain.example" +
                              base::NumberToString(i) + ".com");
      }
    }

    // Expose the first `kNumBenchmarkOrigins` origins for our fast matching
    // benchmarks.
    test_origins_.assign(all_domains.begin(),
                         all_domains.begin() + kNumBenchmarkOrigins);

    // 1. Load production URL rules from the unindexed file
    std::vector<url_pattern_index::proto::UrlRule> url_rules;
    base::File unindexed_file(base::MakeAbsoluteFilePath(unindexed_path_),
                              base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(unindexed_file.IsValid());
    CopyingFileInputStream copying_stream(std::move(unindexed_file));
    google::protobuf::io::CopyingInputStreamAdaptor zero_copy_stream_adaptor(
        &copying_stream, 4096 /* buffer_size */);
    UnindexedRulesetReader reader(&zero_copy_stream_adaptor);
    url_pattern_index::proto::FilteringRules ruleset_chunk;
    while (reader.ReadNextChunk(&ruleset_chunk)) {
      for (const auto& rule : ruleset_chunk.url_rules()) {
        url_rules.push_back(rule);
      }
    }

    // 2. Generate synthetic style rules modeled after EasyList 202604231821
    std::vector<url_pattern_index::proto::StyleRule> style_rules;
    // Domain-specific hiding (actual: 9,717, distributed over all 9,600
    // domains)
    for (int i = 0; i < 9717; ++i) {
      style_rules.push_back(testing::CreateStyleRule(
          testing::StyleRuleParams()
              .SetSelector(".rule-" + base::NumberToString(i))
              .SetDomains({all_domains[i % all_domains.size()]})));
    }

    // Exception rules (actual: 339, whitelisting some of the global class
    // rules)
    for (int i = 0; i < 339; ++i) {
      std::string cls = "class-" + base::NumberToString(i);
      style_rules.push_back(testing::CreateStyleRule(
          testing::StyleRuleParams()
              .SetSelector("." + cls)
              .SetDomains({all_domains[(i * 28) % all_domains.size()]})
              .SetExclusion(true)
              .SetClasses({cls})));
    }

    // Generic ID hiding (actual: 4,267)
    for (int i = 0; i < 4267; ++i) {
      std::string id = "id-" + base::NumberToString(i);
      style_rules.push_back(testing::CreateStyleRule(
          testing::StyleRuleParams().SetSelector("#" + id).SetIds({id})));
    }

    // Generic class hiding (actual: 8,968)
    for (int i = 0; i < 8968; ++i) {
      std::string cls = "class-" + base::NumberToString(i);
      style_rules.push_back(testing::CreateStyleRule(
          testing::StyleRuleParams().SetSelector("." + cls).SetClasses({cls})));
    }

    // Complex rules (nested) (actual global complex: 55)
    for (int i = 0; i < 55; ++i) {
      std::string cls = "c-" + base::NumberToString(i);
      style_rules.push_back(
          testing::CreateStyleRule(testing::StyleRuleParams()
                                       .SetSelector(".a .b ." + cls)
                                       .SetClasses({cls})));
    }

    // 3. Write all of them into a single unindexed protobuf file
    base::File synthetic_unindexed_file(
        unindexed_with_style_path_,
        base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    ASSERT_TRUE(synthetic_unindexed_file.IsValid());
    CopyingFileOutputStream copying_out_stream(
        std::move(synthetic_unindexed_file));
    google::protobuf::io::CopyingOutputStreamAdaptor
        zero_copy_out_stream_adaptor(&copying_out_stream, 4096);
    UnindexedRulesetWriter writer(&zero_copy_out_stream_adaptor);
    for (const auto& rule : url_rules) {
      writer.AddUrlRule(rule);
    }
    for (const auto& rule : style_rules) {
      writer.AddStyleRule(rule);
    }
    writer.Finish();

    // 4. Index and write this merged file to indexed_path_
    ASSERT_TRUE(
        IndexAndWriteRuleset(unindexed_with_style_path_, indexed_path_));

    // 5. Map the unified ruleset
    base::File indexed_file = base::File(
        indexed_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(indexed_file.IsValid());
    ruleset_ = subresource_filter::MemoryMappedRuleset::CreateAndInitialize(
        std::move(indexed_file));

    filter_tool_ = std::make_unique<FilterTool>(ruleset_, &output_);
  }

  FilterTool* filter_tool() { return filter_tool_.get(); }
  const MemoryMappedRuleset* ruleset() const { return ruleset_.get(); }

  const std::string& requests() const { return requests_; }

  const base::FilePath& unindexed_path() const { return unindexed_path_; }
  const base::FilePath& unindexed_with_style_path() const {
    return unindexed_with_style_path_;
  }

  perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
    perf_test::PerfResultReporter reporter("IndexedRuleset.", story_name);
    reporter.RegisterImportantMetric(kMetricIndexAndWriteTimeUs, "us");
    reporter.RegisterImportantMetric(kMetricIndexingTimeUs, "us");
    reporter.RegisterImportantMetric(kMetricMedianMatchTimeUs, "us");
    reporter.RegisterImportantMetric(kMetricStyleMatchTimeUs, "us");
    return reporter;
  }

 protected:
  // Pre-allocated synthetic origins for fast and self-contained style matching
  // tests
  std::vector<std::string> test_origins_;

 private:
  base::ScopedTempDir scoped_dir_;
  base::FilePath unindexed_path_;
  base::FilePath indexed_path_;
  base::FilePath unindexed_with_style_path_;

  std::string requests_;

  // Use an unopened output stream as a sort of null stream. All writes will
  // fail so things should be a bit faster than writing to a string.
  std::ofstream output_;

  std::unique_ptr<FilterTool> filter_tool_;
  scoped_refptr<const MemoryMappedRuleset> ruleset_;
};

TEST_F(IndexedRulesetPerftest, IndexRuleset) {
  std::vector<int64_t> results;
  for (int i = 0; i < 10; ++i) {
    base::ScopedTempDir scoped_dir;
    ASSERT_TRUE(scoped_dir.CreateUniqueTempDir());
    base::FilePath indexed_path =
        scoped_dir.GetPath().AppendASCII("IndexedRuleset");

    base::ElapsedTimer timer;
    ASSERT_TRUE(IndexAndWriteRuleset(unindexed_path(), indexed_path));
    results.emplace_back(timer.Elapsed().InMicroseconds());
  }
  std::sort(results.begin(), results.end());
  perf_test::PerfResultReporter reporter = SetUpReporter("IndexRuleset");
  reporter.AddResult(kMetricIndexAndWriteTimeUs,
                     static_cast<size_t>(results[5]));
}

TEST_F(IndexedRulesetPerftest, Indexing) {
  std::vector<int64_t> results;
  for (int i = 0; i < 10; ++i) {
    base::ScopedTempDir scoped_dir;
    ASSERT_TRUE(scoped_dir.CreateUniqueTempDir());
    base::FilePath indexed_path =
        scoped_dir.GetPath().AppendASCII("IndexedRuleset");

    base::ElapsedTimer timer;
    ASSERT_TRUE(
        IndexAndWriteRuleset(unindexed_with_style_path(), indexed_path));
    results.emplace_back(timer.Elapsed().InMicroseconds());
  }
  std::sort(results.begin(), results.end());
  perf_test::PerfResultReporter reporter = SetUpReporter("Indexing");
  reporter.AddResult(kMetricIndexingTimeUs, static_cast<size_t>(results[5]));
}

TEST_F(IndexedRulesetPerftest, MatchAll) {
  std::vector<int64_t> results;
  for (int i = 0; i < 5; ++i) {
    base::ElapsedTimer timer;
    std::istringstream request_stream(requests());
    filter_tool()->MatchBatch(&request_stream);
    results.emplace_back(timer.Elapsed().InMicroseconds());
  }
  std::sort(results.begin(), results.end());
  perf_test::PerfResultReporter reporter = SetUpReporter("MatchAll");
  reporter.AddResult(kMetricMedianMatchTimeUs, static_cast<size_t>(results[2]));
}

TEST_F(IndexedRulesetPerftest, MatchStyleRulesDomain) {
  std::vector<int64_t> results;
  mojom::ActivationState state;
  state.activation_level = mojom::ActivationLevel::kEnabled;

  std::vector<std::unique_ptr<DocumentSubresourceFilter>> filters;
  for (const auto& origin_str : test_origins_) {
    filters.push_back(std::make_unique<DocumentSubresourceFilter>(
        url::Origin::Create(GURL(origin_str)), state, ruleset(), ""));
  }

  for (int i = 0; i < 5; ++i) {
    base::ElapsedTimer timer;
    for (const auto& filter : filters) {
      std::vector<std::string_view> rules;
      filter->GetDomainSelectors(rules);
    }
    results.emplace_back(timer.Elapsed().InMicroseconds());
  }
  std::sort(results.begin(), results.end());
  perf_test::PerfResultReporter reporter =
      SetUpReporter("MatchStyleRulesDomain");
  reporter.AddResult(kMetricStyleMatchTimeUs, static_cast<size_t>(results[2]));
}

TEST_F(IndexedRulesetPerftest, MatchStyleRulesClass) {
  std::vector<int64_t> results;
  mojom::ActivationState state;
  state.activation_level = mojom::ActivationLevel::kEnabled;

  std::vector<std::unique_ptr<DocumentSubresourceFilter>> filters;
  for (const auto& origin_str : test_origins_) {
    filters.push_back(std::make_unique<DocumentSubresourceFilter>(
        url::Origin::Create(GURL(origin_str)), state, ruleset(), ""));
  }

  for (int i = 0; i < 5; ++i) {
    base::ElapsedTimer timer;
    for (const auto& filter : filters) {
      for (int j = 0; j < kNumLookupsPerOrigin; ++j) {
        std::vector<std::string_view> rules;
        std::string class_name = "class-" + base::NumberToString(j);
        filter->GetSelectorsByClass(
            class_name, subresource_filter::GetStyleRuleHash(class_name),
            rules);
      }
    }
    results.emplace_back(timer.Elapsed().InMicroseconds());
  }
  std::sort(results.begin(), results.end());
  perf_test::PerfResultReporter reporter =
      SetUpReporter("MatchStyleRulesClass");
  reporter.AddResult(kMetricStyleMatchTimeUs, static_cast<size_t>(results[2]));
}

TEST_F(IndexedRulesetPerftest, MatchStyleRulesClassMiss) {
  std::vector<int64_t> results;
  mojom::ActivationState state;
  state.activation_level = mojom::ActivationLevel::kEnabled;

  std::vector<std::unique_ptr<DocumentSubresourceFilter>> filters;
  for (const auto& origin_str : test_origins_) {
    filters.push_back(std::make_unique<DocumentSubresourceFilter>(
        url::Origin::Create(GURL(origin_str)), state, ruleset(), ""));
  }

  for (int i = 0; i < 5; ++i) {
    base::ElapsedTimer timer;
    for (const auto& filter : filters) {
      for (int j = 0; j < kNumLookupsPerOrigin; ++j) {
        std::vector<std::string_view> rules;
        std::string class_name = "nonexistent-class-" + base::NumberToString(j);
        filter->GetSelectorsByClass(
            class_name, subresource_filter::GetStyleRuleHash(class_name),
            rules);
      }
    }
    results.emplace_back(timer.Elapsed().InMicroseconds());
  }
  std::sort(results.begin(), results.end());
  perf_test::PerfResultReporter reporter =
      SetUpReporter("MatchStyleRulesClassMiss");
  reporter.AddResult(kMetricStyleMatchTimeUs, static_cast<size_t>(results[2]));
}

TEST_F(IndexedRulesetPerftest, MatchStyleRulesId) {
  std::vector<int64_t> results;
  mojom::ActivationState state;
  state.activation_level = mojom::ActivationLevel::kEnabled;

  std::vector<std::unique_ptr<DocumentSubresourceFilter>> filters;
  for (const auto& origin_str : test_origins_) {
    filters.push_back(std::make_unique<DocumentSubresourceFilter>(
        url::Origin::Create(GURL(origin_str)), state, ruleset(), ""));
  }

  for (int i = 0; i < 5; ++i) {
    base::ElapsedTimer timer;
    for (const auto& filter : filters) {
      for (int j = 0; j < kNumLookupsPerOrigin; ++j) {
        std::vector<std::string_view> rules;
        std::string id_name = "id-" + base::NumberToString(j);
        filter->GetSelectorsById(
            id_name, subresource_filter::GetStyleRuleHash(id_name), rules);
      }
    }
    results.emplace_back(timer.Elapsed().InMicroseconds());
  }
  std::sort(results.begin(), results.end());
  perf_test::PerfResultReporter reporter = SetUpReporter("MatchStyleRulesId");
  reporter.AddResult(kMetricStyleMatchTimeUs, static_cast<size_t>(results[2]));
}

TEST_F(IndexedRulesetPerftest, MatchStyleRulesIdMiss) {
  std::vector<int64_t> results;
  mojom::ActivationState state;
  state.activation_level = mojom::ActivationLevel::kEnabled;

  std::vector<std::unique_ptr<DocumentSubresourceFilter>> filters;
  for (const auto& origin_str : test_origins_) {
    filters.push_back(std::make_unique<DocumentSubresourceFilter>(
        url::Origin::Create(GURL(origin_str)), state, ruleset(), ""));
  }

  for (int i = 0; i < 5; ++i) {
    base::ElapsedTimer timer;
    for (const auto& filter : filters) {
      for (int j = 0; j < kNumLookupsPerOrigin; ++j) {
        std::vector<std::string_view> rules;
        std::string id_name = "nonexistent-id-" + base::NumberToString(j);
        filter->GetSelectorsById(
            id_name, subresource_filter::GetStyleRuleHash(id_name), rules);
      }
    }
    results.emplace_back(timer.Elapsed().InMicroseconds());
  }
  std::sort(results.begin(), results.end());
  perf_test::PerfResultReporter reporter =
      SetUpReporter("MatchStyleRulesIdMiss");
  reporter.AddResult(kMetricStyleMatchTimeUs, static_cast<size_t>(results[2]));
}

}  // namespace subresource_filter
