// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hints_component_util.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/version.h"
#include "components/optimization_guide/core/bloom_filter.h"
#include "components/optimization_guide/core/hints_component_info.h"
#include "components/optimization_guide/core/optimization_filter.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace optimization_guide {

const base::FilePath::CharType kFileName[] = FILE_PATH_LITERAL("somefile.pb");

class HintsComponentUtilTest : public testing::Test {
 public:
  HintsComponentUtilTest() = default;

  HintsComponentUtilTest(const HintsComponentUtilTest&) = delete;
  HintsComponentUtilTest& operator=(const HintsComponentUtilTest&) = delete;

  ~HintsComponentUtilTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void WriteConfigToFile(const base::FilePath& filePath,
                         const proto::Configuration& config) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_TRUE(base::WriteFile(filePath, serialized_config));
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(HintsComponentUtilTest, RecordProcessHintsComponentResult) {
  base::HistogramTester histogram_tester;
  RecordProcessHintsComponentResult(ProcessHintsComponentResult::kSuccess);
  histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                      ProcessHintsComponentResult::kSuccess, 1);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentInvalidVersion) {
  ProcessHintsComponentResult result;
  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version(""), base::FilePath(kFileName)),
      &result);

  EXPECT_FALSE(config);
  EXPECT_EQ(ProcessHintsComponentResult::kFailedInvalidParameters, result);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentInvalidPath) {
  ProcessHintsComponentResult result;
  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version("1.0.0.0"), base::FilePath()), &result);

  EXPECT_FALSE(config);
  EXPECT_EQ(ProcessHintsComponentResult::kFailedInvalidParameters, result);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentInvalidFile) {
  ProcessHintsComponentResult result;
  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version("1.0.0"), base::FilePath(kFileName)),
      &result);

  EXPECT_FALSE(config);
  EXPECT_EQ(ProcessHintsComponentResult::kFailedReadingFile, result);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentNotAConfigInFile) {
  const base::FilePath filePath = temp_dir().Append(kFileName);
  ASSERT_TRUE(base::WriteFile(filePath, "boo"));

  ProcessHintsComponentResult result;
  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version("1.0.0"), filePath), &result);

  EXPECT_FALSE(config);
  EXPECT_EQ(ProcessHintsComponentResult::kFailedInvalidConfiguration, result);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentSuccess) {
  const base::FilePath filePath = temp_dir().Append(kFileName);
  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("google.com");
  ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(filePath, config));

  ProcessHintsComponentResult result;
  std::unique_ptr<proto::Configuration> processed_config =
      ProcessHintsComponent(
          HintsComponentInfo(base::Version("1.0.0"), filePath), &result);

  ASSERT_TRUE(processed_config);
  EXPECT_EQ(1, processed_config->hints_size());
  EXPECT_EQ("google.com", processed_config->hints()[0].key());
  EXPECT_EQ(ProcessHintsComponentResult::kSuccess, result);
}

TEST_F(HintsComponentUtilTest,
       ProcessHintsComponentSuccessNoResultProvidedDoesntCrash) {
  const base::FilePath filePath = temp_dir().Append(kFileName);
  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("google.com");
  ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(filePath, config));

  std::unique_ptr<proto::Configuration> processed_config =
      ProcessHintsComponent(
          HintsComponentInfo(base::Version("1.0.0"), filePath), nullptr);

  ASSERT_TRUE(processed_config);
  EXPECT_EQ(1, processed_config->hints_size());
  EXPECT_EQ("google.com", processed_config->hints()[0].key());
}

TEST_F(HintsComponentUtilTest, RecordOptimizationFilterStatus) {
  base::HistogramTester histogram_tester;
  RecordOptimizationFilterStatus(
      proto::OptimizationType::NOSCRIPT,
      OptimizationFilterStatus::kFoundServerFilterConfig);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OptimizationFilterStatus.NoScript",
      OptimizationFilterStatus::kFoundServerFilterConfig, 1);

  // Record again with a different suffix to make sure it doesn't choke.
  RecordOptimizationFilterStatus(
      proto::OptimizationType::DEFER_ALL_SCRIPT,
      OptimizationFilterStatus::kFoundServerFilterConfig);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OptimizationFilterStatus.DeferAllScript",
      OptimizationFilterStatus::kFoundServerFilterConfig, 1);
}

TEST_F(HintsComponentUtilTest, ProcessOptimizationFilter) {
  int num_hash_functions = 7;
  int num_bits = 1234;

  proto::OptimizationFilter optimization_filter_proto;
  BloomFilter bloom_filter(num_hash_functions, num_bits);
  bloom_filter.Add("host.com");
  proto::BloomFilter* bloom_filter_proto =
      optimization_filter_proto.mutable_bloom_filter();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  std::string blocklist_data(
      reinterpret_cast<const char*>(&bloom_filter.bytes()[0]),
      bloom_filter.bytes().size());
  bloom_filter_proto->set_data(blocklist_data);

  OptimizationFilterStatus status;
  std::unique_ptr<OptimizationFilter> optimization_filter =
      ProcessOptimizationFilter(optimization_filter_proto, &status);

  EXPECT_EQ(status, OptimizationFilterStatus::kCreatedServerFilter);
  ASSERT_TRUE(optimization_filter);
  EXPECT_TRUE(optimization_filter->Matches(GURL("https://m.host.com")));
}

TEST_F(HintsComponentUtilTest, ProcessOptimizationFilterWithHashedInput) {
  int num_hash_functions = 7;
  int num_bits = 1234;

  proto::OptimizationFilter optimization_filter_proto;
  optimization_filter_proto.set_bloom_filter_format(
      proto::BLOOM_FILTER_FORMAT_SHA256);
  BloomFilter bloom_filter(num_hash_functions, num_bits);
  // google.com
  bloom_filter.Add(
      "D4C9D9027326271A89CE51FCAF328ED673F17BE33469FF979E8AB8DD501E664F");
  proto::BloomFilter* bloom_filter_proto =
      optimization_filter_proto.mutable_bloom_filter();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  std::string blocklist_data(
      reinterpret_cast<const char*>(&bloom_filter.bytes()[0]),
      bloom_filter.bytes().size());
  bloom_filter_proto->set_data(blocklist_data);

  OptimizationFilterStatus status;
  std::unique_ptr<OptimizationFilter> optimization_filter =
      ProcessOptimizationFilter(optimization_filter_proto, &status);

  EXPECT_EQ(status, OptimizationFilterStatus::kCreatedServerFilter);
  ASSERT_TRUE(optimization_filter);
  EXPECT_FALSE(optimization_filter->Matches(GURL("https://m.host.com")));
  EXPECT_TRUE(optimization_filter->Matches(GURL("https://google.com")));
}

TEST_F(HintsComponentUtilTest, ProcessOptimizationFilterWithBadNumBits) {
  proto::OptimizationFilter optimization_filter_proto;
  BloomFilter bloom_filter(7, 1234);
  bloom_filter.Add("host.com");
  proto::BloomFilter* bloom_filter_proto =
      optimization_filter_proto.mutable_bloom_filter();
  bloom_filter_proto->set_num_hash_functions(7);
  bloom_filter_proto->set_num_bits(bloom_filter.bytes().size() * 8 + 1);
  std::string blocklist_data(
      reinterpret_cast<const char*>(&bloom_filter.bytes()[0]),
      bloom_filter.bytes().size());
  bloom_filter_proto->set_data(blocklist_data);

  OptimizationFilterStatus status;
  std::unique_ptr<OptimizationFilter> optimization_filter =
      ProcessOptimizationFilter(optimization_filter_proto, &status);

  EXPECT_EQ(status, OptimizationFilterStatus::kFailedServerFilterBadConfig);
  EXPECT_EQ(nullptr, optimization_filter);
}

TEST_F(HintsComponentUtilTest, ProcessOptimizationFilterWithRegexps) {
  proto::OptimizationFilter optimization_filter_proto;
  optimization_filter_proto.add_regexps("test");

  OptimizationFilterStatus status;
  std::unique_ptr<OptimizationFilter> optimization_filter =
      ProcessOptimizationFilter(optimization_filter_proto, &status);

  EXPECT_EQ(status, OptimizationFilterStatus::kCreatedServerFilter);
  ASSERT_TRUE(optimization_filter);
  EXPECT_TRUE(optimization_filter->Matches(GURL("https://test.com")));
}

TEST_F(HintsComponentUtilTest, ProcessOptimizationFilterWithExclusionRegexps) {
  proto::OptimizationFilter optimization_filter_proto;
  optimization_filter_proto.add_exclusion_regexps("test");

  OptimizationFilterStatus status;
  std::unique_ptr<OptimizationFilter> optimization_filter =
      ProcessOptimizationFilter(optimization_filter_proto, &status);

  EXPECT_EQ(status, OptimizationFilterStatus::kCreatedServerFilter);
  ASSERT_TRUE(optimization_filter);
  EXPECT_FALSE(optimization_filter->Matches(GURL("https://test.com")));
}

TEST_F(HintsComponentUtilTest, ProcessOptimizationFilterWithInvalidRegexps) {
  proto::OptimizationFilter optimization_filter_proto;
  optimization_filter_proto.add_regexps("test[");

  OptimizationFilterStatus status;
  std::unique_ptr<OptimizationFilter> optimization_filter =
      ProcessOptimizationFilter(optimization_filter_proto, &status);

  EXPECT_EQ(status, OptimizationFilterStatus::kInvalidRegexp);
  EXPECT_EQ(nullptr, optimization_filter);
}

TEST_F(HintsComponentUtilTest,
       ProcessOptimizationFilterInvalidRegexpsOverridesBloomFilterStatus) {
  int num_hash_functions = 7;
  int num_bits = 1234;

  proto::OptimizationFilter optimization_filter_proto;
  optimization_filter_proto.add_regexps("test[");
  BloomFilter bloom_filter(num_hash_functions, num_bits);
  bloom_filter.Add("host.com");
  proto::BloomFilter* bloom_filter_proto =
      optimization_filter_proto.mutable_bloom_filter();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  std::string blocklist_data(
      reinterpret_cast<const char*>(&bloom_filter.bytes()[0]),
      bloom_filter.bytes().size());
  bloom_filter_proto->set_data(blocklist_data);

  OptimizationFilterStatus status;
  std::unique_ptr<OptimizationFilter> optimization_filter =
      ProcessOptimizationFilter(optimization_filter_proto, &status);

  EXPECT_EQ(status, OptimizationFilterStatus::kInvalidRegexp);
  EXPECT_EQ(nullptr, optimization_filter);
}

TEST_F(
    HintsComponentUtilTest,
    ProcessOptimizationFilterInvalidExclusionRegexpsOverridesBloomFilterStatus) {
  int num_hash_functions = 7;
  int num_bits = 1234;

  proto::OptimizationFilter optimization_filter_proto;
  optimization_filter_proto.add_exclusion_regexps("test[");
  BloomFilter bloom_filter(num_hash_functions, num_bits);
  bloom_filter.Add("host.com");
  proto::BloomFilter* bloom_filter_proto =
      optimization_filter_proto.mutable_bloom_filter();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  std::string blocklist_data(
      reinterpret_cast<const char*>(&bloom_filter.bytes()[0]),
      bloom_filter.bytes().size());
  bloom_filter_proto->set_data(blocklist_data);

  OptimizationFilterStatus status;
  std::unique_ptr<OptimizationFilter> optimization_filter =
      ProcessOptimizationFilter(optimization_filter_proto, &status);

  EXPECT_EQ(status, OptimizationFilterStatus::kInvalidRegexp);
  EXPECT_EQ(nullptr, optimization_filter);
}

TEST_F(HintsComponentUtilTest, ProcessOptimizationFilterWithTooLargeFilter) {
  int too_many_bits = features::MaxServerBloomFilterByteSize() * 8 + 1;

  proto::OptimizationFilter optimization_filter_proto;
  BloomFilter bloom_filter(7, too_many_bits);
  bloom_filter.Add("host.com");
  proto::BloomFilter* bloom_filter_proto =
      optimization_filter_proto.mutable_bloom_filter();
  bloom_filter_proto->set_num_hash_functions(7);
  bloom_filter_proto->set_num_bits(too_many_bits);
  std::string blocklist_data(
      reinterpret_cast<const char*>(&bloom_filter.bytes()[0]),
      bloom_filter.bytes().size());
  bloom_filter_proto->set_data(blocklist_data);

  OptimizationFilterStatus status;
  std::unique_ptr<OptimizationFilter> optimization_filter =
      ProcessOptimizationFilter(optimization_filter_proto, &status);

  EXPECT_EQ(status, OptimizationFilterStatus::kFailedServerFilterTooBig);
  EXPECT_EQ(nullptr, optimization_filter);
}

TEST_F(HintsComponentUtilTest,
       ProcessOptimizationFilterNoResultProvidedDoesntCrash) {
  int num_hash_functions = 7;
  int num_bits = 1234;

  proto::OptimizationFilter optimization_filter_proto;
  BloomFilter bloom_filter(num_hash_functions, num_bits);
  bloom_filter.Add("host.com");
  proto::BloomFilter* bloom_filter_proto =
      optimization_filter_proto.mutable_bloom_filter();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  std::string blocklist_data(
      reinterpret_cast<const char*>(&bloom_filter.bytes()[0]),
      bloom_filter.bytes().size());
  bloom_filter_proto->set_data(blocklist_data);

  std::unique_ptr<OptimizationFilter> optimization_filter =
      ProcessOptimizationFilter(optimization_filter_proto,
                                /*out_status=*/nullptr);

  ASSERT_TRUE(optimization_filter);
  EXPECT_TRUE(optimization_filter->Matches(GURL("https://m.host.com")));
}

TEST_F(HintsComponentUtilTest,
       ProcessOptimizationFilterSkipHostSuffixCheckingIsPropagated) {
  int num_hash_functions = 7;
  int num_bits = 1234;

  proto::OptimizationFilter optimization_filter_proto;
  optimization_filter_proto.set_skip_host_suffix_checking(true);
  BloomFilter bloom_filter(num_hash_functions, num_bits);
  bloom_filter.Add("host.com");
  proto::BloomFilter* bloom_filter_proto =
      optimization_filter_proto.mutable_bloom_filter();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  std::string blocklist_data(
      reinterpret_cast<const char*>(&bloom_filter.bytes()[0]),
      bloom_filter.bytes().size());
  bloom_filter_proto->set_data(blocklist_data);

  std::unique_ptr<OptimizationFilter> optimization_filter =
      ProcessOptimizationFilter(optimization_filter_proto,
                                /*out_status=*/nullptr);

  ASSERT_TRUE(optimization_filter);
  EXPECT_FALSE(optimization_filter->Matches(GURL("https://m.host.com")));
}

}  // namespace optimization_guide
