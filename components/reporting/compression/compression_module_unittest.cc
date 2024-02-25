// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/compression/compression_module.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/snappy/src/snappy.h"

using ::testing::Eq;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kTestString[] = "AAAAAAAAAAAAAA1111111111111";
constexpr char kPoorlyCompressibleTestString[] = "AAAAA11111";

class CompressionModuleTest : public ::testing::Test {
 protected:
  CompressionModuleTest()
      : memory_resource_(base::MakeRefCounted<ResourceManager>(
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

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<ResourceManager> memory_resource_;
  scoped_refptr<CompressionModule> compression_module_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CompressionModuleTest, CompressRecordSnappy) {
  EnableCompression();
  scoped_refptr<CompressionModule> test_compression_module =
      CompressionModule::Create(0, CompressionInformation::COMPRESSION_SNAPPY);

  // Compress string directly with snappy as benchmark
  const std::string expected_output =
      BenchmarkCompressRecordSnappy(kTestString);

  test::TestMultiEvent<std::string, std::optional<CompressionInformation>>
      compressed_record_event;
  // Compress string with CompressionModule
  test_compression_module->CompressRecord(kTestString, memory_resource_,
                                          compressed_record_event.cb());

  const std::tuple<std::string, std::optional<CompressionInformation>>
      compressed_record_tuple = compressed_record_event.result();

  const std::string_view compressed_string_callback =
      std::get<0>(compressed_record_tuple);

  // Expect that benchmark compression is the same as compression module
  EXPECT_THAT(compressed_string_callback, StrEq(expected_output));

  const std::optional<CompressionInformation> compression_info =
      std::get<1>(compressed_record_tuple);

  EXPECT_TRUE(compression_info.has_value());

  // Expect that compression information contains COMPRESSION_SNAPPY
  EXPECT_THAT(compression_info.value().compression_algorithm(),
              Eq(CompressionInformation::COMPRESSION_SNAPPY));
}

TEST_F(CompressionModuleTest, CompressPoorlyCompressibleRecordSnappy) {
  EnableCompression();
  scoped_refptr<CompressionModule> test_compression_module =
      CompressionModule::Create(0, CompressionInformation::COMPRESSION_SNAPPY);

  // Compress string directly with snappy as benchmark
  const std::string expected_output =
      BenchmarkCompressRecordSnappy(kPoorlyCompressibleTestString);

  test::TestMultiEvent<std::string, std::optional<CompressionInformation>>
      compressed_record_event;
  // Compress string with CompressionModule
  test_compression_module->CompressRecord(kPoorlyCompressibleTestString,
                                          memory_resource_,
                                          compressed_record_event.cb());

  const std::tuple<std::string, std::optional<CompressionInformation>>
      compressed_record_tuple = compressed_record_event.result();

  const std::string_view compressed_string_callback =
      std::get<0>(compressed_record_tuple);

  // Expect that benchmark compression is the same as compression module
  EXPECT_THAT(compressed_string_callback, StrEq(kPoorlyCompressibleTestString));

  const std::optional<CompressionInformation> compression_info =
      std::get<1>(compressed_record_tuple);

  EXPECT_TRUE(compression_info.has_value());

  // Expect that compression information contains COMPRESSION_NONE
  EXPECT_THAT(compression_info.value().compression_algorithm(),
              Eq(CompressionInformation::COMPRESSION_NONE));
}

TEST_F(CompressionModuleTest, CompressRecordBelowThreshold) {
  EnableCompression();
  scoped_refptr<CompressionModule> test_compression_module =
      CompressionModule::Create(512,
                                CompressionInformation::COMPRESSION_SNAPPY);

  test::TestMultiEvent<std::string, std::optional<CompressionInformation>>
      compressed_record_event;
  // Compress string with CompressionModule
  test_compression_module->CompressRecord(kTestString, memory_resource_,
                                          compressed_record_event.cb());

  const std::tuple<std::string, std::optional<CompressionInformation>>
      compressed_record_tuple = compressed_record_event.result();

  const std::string_view compressed_string_callback =
      std::get<0>(compressed_record_tuple);

  // Expect that record is not compressed since size is smaller than 512 bytes
  EXPECT_THAT(compressed_string_callback, StrEq(kTestString));

  const std::optional<CompressionInformation> compression_info =
      std::get<1>(compressed_record_tuple);

  EXPECT_TRUE(compression_info.has_value());

  // Expect that compression information contains COMPRESSION_NONE since the
  // record was below the compression threshold.
  EXPECT_THAT(compression_info.value().compression_algorithm(),
              Eq(CompressionInformation::COMPRESSION_NONE));
}

TEST_F(CompressionModuleTest, CompressRecordCompressionDisabled) {
  // Disable compression feature
  DisableCompression();
  scoped_refptr<CompressionModule> test_compression_module =
      CompressionModule::Create(0, CompressionInformation::COMPRESSION_SNAPPY);

  test::TestMultiEvent<std::string, std::optional<CompressionInformation>>
      compressed_record_event;

  // Compress string with CompressionModule
  test_compression_module->CompressRecord(kTestString, memory_resource_,
                                          compressed_record_event.cb());

  const std::tuple<std::string, std::optional<CompressionInformation>>
      compressed_record_tuple = compressed_record_event.result();

  const std::string_view compressed_string_callback =
      std::get<0>(compressed_record_tuple);

  // Expect that record is not compressed since compression is not enabled
  EXPECT_THAT(compressed_string_callback, StrEq(kTestString));

  const std::optional<CompressionInformation> compression_info =
      std::get<1>(compressed_record_tuple);

  // Expect no compression information since compression has been disabled.
  EXPECT_FALSE(compression_info.has_value());
}

TEST_F(CompressionModuleTest, CompressRecordCompressionNone) {
  EnableCompression();
  scoped_refptr<CompressionModule> test_compression_module =
      CompressionModule::Create(0, CompressionInformation::COMPRESSION_NONE);

  test::TestMultiEvent<std::string, std::optional<CompressionInformation>>
      compressed_record_event;

  // Compress string with CompressionModule
  test_compression_module->CompressRecord(kTestString, memory_resource_,
                                          compressed_record_event.cb());
  const std::tuple<std::string, std::optional<CompressionInformation>>
      compressed_record_tuple = compressed_record_event.result();

  const std::string_view compressed_string_callback =
      std::get<0>(compressed_record_tuple);

  // Expect that record is not compressed since COMPRESSION_NONE was chosen as
  // the compression_algorithm.
  EXPECT_THAT(compressed_string_callback, StrEq(kTestString));

  const std::optional<CompressionInformation> compression_info =
      std::get<1>(compressed_record_tuple);

  EXPECT_TRUE(compression_info.has_value());

  // Expect that compression information contains COMPRESSION_NONE
  EXPECT_THAT(compression_info.value().compression_algorithm(),
              Eq(CompressionInformation::COMPRESSION_NONE));
}
}  // namespace
}  // namespace reporting
