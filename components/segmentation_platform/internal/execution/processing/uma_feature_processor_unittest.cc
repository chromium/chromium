// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/uma_feature_processor.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/signal_database_impl.h"
#include "components/segmentation_platform/internal/execution/processing/feature_aggregator_impl.h"
#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/internal/execution/processing/query_processor.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/proto/signal.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "feature_aggregator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::processing {

constexpr std::array<int32_t, 2> kEnumHistorgram0And1{0, 1};

constexpr MetadataWriter::UMAFeature kEnumRecorded4Times =
    MetadataWriter::UMAFeature::FromEnumHistogram("EnumRecorded4Times",
                                                  28,
                                                  kEnumHistorgram0And1.data(),
                                                  kEnumHistorgram0And1.size());
constexpr MetadataWriter::UMAFeature kEnumRecorded4TimesAsValue =
    MetadataWriter::UMAFeature::FromValueHistogram("EnumRecorded4Times",
                                                   28,
                                                   proto::Aggregation::COUNT);

constexpr MetadataWriter::UMAFeature kUserActionTwice =
    MetadataWriter::UMAFeature::FromUserAction("UserActionTwice", 28);

constexpr MetadataWriter::UMAFeature kUserActionOnce =
    MetadataWriter::UMAFeature::FromUserAction("UserActionOnce", 28);

constexpr MetadataWriter::UMAFeature kValueHistogram =
    MetadataWriter::UMAFeature::FromValueHistogram("ValueHistogram",
                                                   28,
                                                   proto::Aggregation::SUM);

constexpr MetadataWriter::UMAFeature kEnumHistogramNotRecorded =
    MetadataWriter::UMAFeature::FromEnumHistogram("EnumHistogramNotRecorded",
                                                  28,
                                                  kEnumHistorgram0And1.data(),
                                                  kEnumHistorgram0And1.size());

constexpr MetadataWriter::UMAFeature kUserActionNotRecorded =
    MetadataWriter::UMAFeature::FromUserAction("UserActionNotRecorded", 28);

constexpr std::array<MetadataWriter::UMAFeature, 7> kUMAFeatures = {
    kEnumRecorded4Times,   kEnumRecorded4TimesAsValue,
    kUserActionTwice,      kUserActionOnce,
    kValueHistogram,       kEnumHistogramNotRecorded,
    kUserActionNotRecorded};

class UmaFeatureProcessorTest : public testing::Test {
 public:
  UmaFeatureProcessorTest() = default;
  ~UmaFeatureProcessorTest() override = default;

  void SetUp() override {
    Test::SetUp();
    clock_.SetNow(base::Time::Now());
    ASSERT_TRUE(
        temp_dir_.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath(), true);

    clock_.Advance(base::Days(1));
    signal_database_ = std::make_unique<SignalDatabaseImpl>(
        db_provider_->GetDB<proto::SignalData>(
            leveldb_proto::ProtoDbType::SIGNAL_DATABASE,
            temp_dir_.GetPath().Append(FILE_PATH_LITERAL("signaldb")),
            task_env_.GetMainThreadTaskRunner()),
        &clock_, task_env_.GetMainThreadTaskRunner());

    base::RunLoop wait_for_init;
    signal_database_->Initialize(base::BindOnce(
        [](base::OnceClosure quit_closure, bool success) {
          ASSERT_TRUE(success);
          std::move(quit_closure).Run();
        },
        wait_for_init.QuitClosure()));
    wait_for_init.Run();
  }

  void TearDown() override {
    signal_database_.reset();
    signal_db_.reset();
    db_provider_.reset();
    task_env_.RunUntilIdle();
    ASSERT_TRUE(temp_dir_.Delete());
    Test::TearDown();
  }

  void AddSignalsToDb() {
    signal_database_->WriteSample(proto::SignalType::HISTOGRAM_ENUM,
                                  base::HashMetricName("EnumRecorded4Times"), 0,
                                  base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::HISTOGRAM_VALUE,
                                  base::HashMetricName("EnumRecorded4Times"), 0,
                                  base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::HISTOGRAM_ENUM,
                                  base::HashMetricName("EnumRecorded4Times"), 0,
                                  base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::HISTOGRAM_VALUE,
                                  base::HashMetricName("EnumRecorded4Times"), 0,
                                  base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::HISTOGRAM_ENUM,
                                  base::HashMetricName("EnumRecorded4Times"), 1,
                                  base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::HISTOGRAM_VALUE,
                                  base::HashMetricName("EnumRecorded4Times"), 1,
                                  base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::HISTOGRAM_ENUM,
                                  base::HashMetricName("EnumRecorded4Times"), 2,
                                  base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::HISTOGRAM_VALUE,
                                  base::HashMetricName("EnumRecorded4Times"), 2,
                                  base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::USER_ACTION,
                                  base::HashMetricName("UserActionTwice"),
                                  absl::nullopt, base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::USER_ACTION,
                                  base::HashMetricName("UserActionTwice"),
                                  absl::nullopt, base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::USER_ACTION,
                                  base::HashMetricName("UserActionOnce"),
                                  absl::nullopt, base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::HISTOGRAM_VALUE,
                                  base::HashMetricName("ValueHistogram"), 34,
                                  base::DoNothing());
    signal_database_->WriteSample(proto::SignalType::HISTOGRAM_VALUE,
                                  base::HashMetricName("ValueHistogram"), 85,
                                  base::DoNothing());
    signal_database_->WriteSample(
        proto::SignalType::HISTOGRAM_VALUE,
        base::HashMetricName("ValueHistogram"), 1800,
        base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  }

  void CreateProcessor(const proto::SegmentationModelMetadata& metadata,
                       const std::vector<float> expected) {
    base::flat_map<QueryProcessor::FeatureIndex, Data> uma_features;
    for (int i = 0; i < metadata.input_features_size(); ++i) {
      ASSERT_TRUE(metadata.input_features(i).has_uma_feature());
      uma_features.emplace(std::make_pair(i, metadata.input_features(i)));
    }
    auto feature_aggregator = std::make_unique<FeatureAggregatorImpl>();
    auto uma_processor = std::make_unique<UmaFeatureProcessor>(
        std::move(uma_features), signal_database_.get(),
        feature_aggregator.get(), clock_.Now(), base::Time(), base::Days(1),
        proto::SegmentId::CROSS_DEVICE_USER_SEGMENT, false);

    base::RunLoop wait_to_process;
    auto feature_state = std::make_unique<FeatureProcessorState>(
        FeatureProcessorStateId::FromUnsafeValue(1), clock_.Now(), base::Time(),
        base::Days(1), proto::SegmentId::CROSS_DEVICE_USER_SEGMENT, nullptr,
        base::DoNothing());
    uma_processor->Process(
        *feature_state, base::BindOnce(
                            [](const std::vector<float>& expected,
                               base::OnceClosure quit_closure,
                               QueryProcessor::IndexedTensors result) {
                              ASSERT_EQ(expected.size(), result.size());
                              for (unsigned i = 0; i < expected.size(); ++i) {
                                ASSERT_EQ(result[i].size(), 1u);
                                EXPECT_NEAR(result[i][0].float_val, expected[i],
                                            0.001);
                              }
                              std::move(quit_closure).Run();
                            },
                            expected, wait_to_process.QuitClosure()));
    wait_to_process.Run();
    EXPECT_FALSE(feature_state->error());
  }

 protected:
  base::test::TaskEnvironment task_env_;
  base::ScopedTempDir temp_dir_;
  base::SimpleTestClock clock_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<leveldb_proto::ProtoDatabase<proto::SignalData>> signal_db_;
  std::unique_ptr<SignalDatabase> signal_database_;
};

TEST_F(UmaFeatureProcessorTest, ProcessEnumFeature) {
  AddSignalsToDb();
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.AddUmaFeatures(&kEnumRecorded4Times, 1);
  CreateProcessor(metadata, {3});
}

constexpr std::array<int32_t, 1> kEnumHistorgram4{4};
constexpr MetadataWriter::UMAFeature kEnumValueNotRecorded =
    MetadataWriter::UMAFeature::FromEnumHistogram("EnumRecorded4Times",
                                                  28,
                                                  kEnumHistorgram4.data(),
                                                  kEnumHistorgram4.size());

TEST_F(UmaFeatureProcessorTest, ProcessEnumWithUnrecordedValues) {
  AddSignalsToDb();
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.AddUmaFeatures(&kEnumValueNotRecorded, 1);
  CreateProcessor(metadata, {0});
}

TEST_F(UmaFeatureProcessorTest, ProcessUserAction) {
  AddSignalsToDb();
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.AddUmaFeatures(&kUserActionTwice, 1);
  CreateProcessor(metadata, {2});
}

TEST_F(UmaFeatureProcessorTest, ProcessValue) {
  AddSignalsToDb();
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.AddUmaFeatures(&kValueHistogram, 1);
  CreateProcessor(metadata, {1919});
}

TEST_F(UmaFeatureProcessorTest, ProcessWithNoData) {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());
  CreateProcessor(metadata, {0, 0, 0, 0, 0, 0, 0});
}

TEST_F(UmaFeatureProcessorTest, ProcessWithDatabase) {
  AddSignalsToDb();
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());
  CreateProcessor(metadata, {3, 4, 2, 1, 1919, 0, 0});
}

}  // namespace segmentation_platform::processing
