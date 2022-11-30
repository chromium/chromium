// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/compression/compression_module.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/memory_resource_impl.h"
#include "components/reporting/resources/resource_interface.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/snappy/src/snappy.h"

using ::testing::Eq;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kTestString[] = "AAAAA11111";

enum class CompressedRecordThresholdMetricEvent {
  kNotCompressed = 0,
  kCompressed = 1,
  kMaxValue = kCompressed
};

constexpr char kCompressionThresholdCountMetricsName[] =
    "Enterprise.CloudReportingCompressionThresholdCount";

constexpr char kSnappyUncompressedRecordSizeMetricsName[] =
    "Enterprise.CloudReportingSnappyUncompressedRecordSize";

constexpr char kSnappyCompressedRecordSizeMetricsName[] =
    "Enterprise.CloudReportingSnappyCompressedRecordSize";

class CompressionModuleTest : public ::testing::Test {
 protected:
  CompressionModuleTest()
      : memory_resource_(base::MakeRefCounted<MemoryResourceImpl>(
            4u * 1024LLu * 1024LLu))  // 4 MiB
  {}

  void TearDown() override { ASSERT_THAT(memory_resource_->GetUsed(), Eq(0u)); }
  std::string BenchmarkCompressRecordSnappy(std::string record_string) {
    std::string output;
    snappy::Compress(record_string.data(), record_string.size(), &output);
    return output;
  }

  void EnableCompression() {
    scoped_feature_list_.InitAndEnableFeature(kCompressReportingPipeline);
  }
  void DisableCompression() {
    scoped_feature_list_.InitAndDisableFeature(kCompressReportingPipeline);
  }

  scoped_refptr<ResourceInterface> memory_resource_;
  scoped_refptr<CompressionModule> compression_module_;
  base::test::TaskEnvironment task_environment_{};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CompressionModuleTest, CompressRecordSnappy) {
  // Poll the task and make sure histograms are logged.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kCompressed, 0);

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kNotCompressed, 0);

  histogram_tester.ExpectTotalCount(kSnappyUncompressedRecordSizeMetricsName,
                                    0);
  histogram_tester.ExpectTotalCount(kSnappyCompressedRecordSizeMetricsName, 0);

  EnableCompression();
  scoped_refptr<CompressionModule> test_compression_module =
      CompressionModule::Create(0, CompressionInformation::COMPRESSION_SNAPPY);

  // Compress string directly with snappy as benchmark
  const std::string expected_output =
      BenchmarkCompressRecordSnappy(kTestString);

  test::TestMultiEvent<std::string, absl::optional<CompressionInformation>>
      compressed_record_event;
  // Compress string with CompressionModule
  test_compression_module->CompressRecord(kTestString, memory_resource_,
                                          compressed_record_event.cb());

  const std::tuple<std::string, absl::optional<CompressionInformation>>
      compressed_record_tuple = compressed_record_event.result();

  const base::StringPiece compressed_string_callback =
      std::get<0>(compressed_record_tuple);

  // Expect that benchmark compression is the same as compression module
  EXPECT_THAT(compressed_string_callback, StrEq(expected_output));

  const absl::optional<CompressionInformation> compression_info =
      std::get<1>(compressed_record_tuple);

  EXPECT_TRUE(compression_info.has_value());

  // Expect that compression information contains COMPRESSION_SNAPPY
  EXPECT_THAT(compression_info.value().compression_algorithm(),
              Eq(CompressionInformation::COMPRESSION_SNAPPY));

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kCompressed, 1);

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kNotCompressed, 0);

  histogram_tester.ExpectTotalCount(kSnappyUncompressedRecordSizeMetricsName,
                                    1);
  histogram_tester.ExpectTotalCount(kSnappyCompressedRecordSizeMetricsName, 1);
}

TEST_F(CompressionModuleTest, CompressRecordBelowThreshold) {
  // Poll the task and make sure histograms are logged.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kCompressed, 0);

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kNotCompressed, 0);

  histogram_tester.ExpectTotalCount(kSnappyUncompressedRecordSizeMetricsName,
                                    0);
  histogram_tester.ExpectTotalCount(kSnappyCompressedRecordSizeMetricsName, 0);

  EnableCompression();
  scoped_refptr<CompressionModule> test_compression_module =
      CompressionModule::Create(512,
                                CompressionInformation::COMPRESSION_SNAPPY);

  test::TestMultiEvent<std::string, absl::optional<CompressionInformation>>
      compressed_record_event;
  // Compress string with CompressionModule
  test_compression_module->CompressRecord(kTestString, memory_resource_,
                                          compressed_record_event.cb());

  const std::tuple<std::string, absl::optional<CompressionInformation>>
      compressed_record_tuple = compressed_record_event.result();

  const base::StringPiece compressed_string_callback =
      std::get<0>(compressed_record_tuple);

  // Expect that record is not compressed since size is smaller than 512 bytes
  EXPECT_THAT(compressed_string_callback, StrEq(kTestString));

  const absl::optional<CompressionInformation> compression_info =
      std::get<1>(compressed_record_tuple);

  EXPECT_TRUE(compression_info.has_value());

  // Expect that compression information contains COMPRESSION_NONE since the
  // record was below the compression threshold.
  EXPECT_THAT(compression_info.value().compression_algorithm(),
              Eq(CompressionInformation::COMPRESSION_NONE));

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kCompressed, 0);

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kNotCompressed, 1);

  histogram_tester.ExpectTotalCount(kSnappyUncompressedRecordSizeMetricsName,
                                    0);
  histogram_tester.ExpectTotalCount(kSnappyCompressedRecordSizeMetricsName, 0);
}

TEST_F(CompressionModuleTest, CompressRecordCompressionDisabled) {
  // Poll the task and make sure histograms are logged.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kCompressed, 0);

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kNotCompressed, 0);

  histogram_tester.ExpectTotalCount(kSnappyUncompressedRecordSizeMetricsName,
                                    0);
  histogram_tester.ExpectTotalCount(kSnappyCompressedRecordSizeMetricsName, 0);

  // Disable compression feature
  DisableCompression();
  scoped_refptr<CompressionModule> test_compression_module =
      CompressionModule::Create(0, CompressionInformation::COMPRESSION_SNAPPY);

  test::TestMultiEvent<std::string, absl::optional<CompressionInformation>>
      compressed_record_event;

  // Compress string with CompressionModule
  test_compression_module->CompressRecord(kTestString, memory_resource_,
                                          compressed_record_event.cb());

  const std::tuple<std::string, absl::optional<CompressionInformation>>
      compressed_record_tuple = compressed_record_event.result();

  const base::StringPiece compressed_string_callback =
      std::get<0>(compressed_record_tuple);

  // Expect that record is not compressed since compression is not enabled
  EXPECT_THAT(compressed_string_callback, StrEq(kTestString));

  const absl::optional<CompressionInformation> compression_info =
      std::get<1>(compressed_record_tuple);

  // Expect no compression information since compression has been disabled.
  EXPECT_FALSE(compression_info.has_value());

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kCompressed, 0);

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kNotCompressed, 0);

  histogram_tester.ExpectTotalCount(kSnappyUncompressedRecordSizeMetricsName,
                                    0);
  histogram_tester.ExpectTotalCount(kSnappyCompressedRecordSizeMetricsName, 0);
}

TEST_F(CompressionModuleTest, CompressRecordCompressionNone) {
  // Poll the task and make sure histograms are logged.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kCompressed, 0);

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kNotCompressed, 0);

  histogram_tester.ExpectTotalCount(kSnappyUncompressedRecordSizeMetricsName,
                                    0);
  histogram_tester.ExpectTotalCount(kSnappyCompressedRecordSizeMetricsName, 0);

  EnableCompression();
  scoped_refptr<CompressionModule> test_compression_module =
      CompressionModule::Create(0, CompressionInformation::COMPRESSION_NONE);

  test::TestMultiEvent<std::string, absl::optional<CompressionInformation>>
      compressed_record_event;

  // Compress string with CompressionModule
  test_compression_module->CompressRecord(kTestString, memory_resource_,
                                          compressed_record_event.cb());
  const std::tuple<std::string, absl::optional<CompressionInformation>>
      compressed_record_tuple = compressed_record_event.result();

  const base::StringPiece compressed_string_callback =
      std::get<0>(compressed_record_tuple);

  // Expect that record is not compressed since COMPRESSION_NONE was chosen as
  // the compression_algorithm.
  EXPECT_THAT(compressed_string_callback, StrEq(kTestString));

  const absl::optional<CompressionInformation> compression_info =
      std::get<1>(compressed_record_tuple);

  EXPECT_TRUE(compression_info.has_value());

  // Expect that compression information contains COMPRESSION_NONE
  EXPECT_THAT(compression_info.value().compression_algorithm(),
              Eq(CompressionInformation::COMPRESSION_NONE));

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kCompressed, 0);

  histogram_tester.ExpectBucketCount(
      kCompressionThresholdCountMetricsName,
      CompressedRecordThresholdMetricEvent::kNotCompressed, 0);

  histogram_tester.ExpectTotalCount(kSnappyUncompressedRecordSizeMetricsName,
                                    0);
  histogram_tester.ExpectTotalCount(kSnappyCompressedRecordSizeMetricsName, 0);
}

}  // namespace
}  // namespace reporting
