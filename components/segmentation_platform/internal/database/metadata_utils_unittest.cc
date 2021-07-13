// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/metadata_utils.h"

#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class MetadataUtilsTest : public testing::Test {
 public:
  ~MetadataUtilsTest() override = default;
};

TEST_F(MetadataUtilsTest, SegmentInfoValidation) {
  proto::SegmentInfo segment_info;
  EXPECT_EQ(metadata_utils::ValidationResult::SEGMENT_ID_NOT_FOUND,
            metadata_utils::ValidateSegmentInfo(segment_info));

  segment_info.set_segment_id(optimization_guide::proto::OptimizationTarget::
                                  OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  EXPECT_EQ(metadata_utils::ValidationResult::METADATA_NOT_FOUND,
            metadata_utils::ValidateSegmentInfo(segment_info));

  // The rest of this test verifies that at least some metadata is verified.
  segment_info.mutable_model_metadata()->set_time_unit(
      proto::UNKNOWN_TIME_UNIT);
  EXPECT_EQ(metadata_utils::ValidationResult::TIME_UNIT_INVALID,
            metadata_utils::ValidateSegmentInfo(segment_info));

  segment_info.mutable_model_metadata()->set_time_unit(proto::DAY);
  EXPECT_EQ(metadata_utils::ValidationResult::VALIDATION_SUCCESS,
            metadata_utils::ValidateSegmentInfo(segment_info));
}

TEST_F(MetadataUtilsTest, DefaultMetadataIsValid) {
  proto::SegmentationModelMetadata empty;

  EXPECT_EQ(metadata_utils::ValidationResult::VALIDATION_SUCCESS,
            metadata_utils::ValidateMetadata(empty));
}

TEST_F(MetadataUtilsTest, MetadataValidation) {
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::UNKNOWN_TIME_UNIT);
  EXPECT_EQ(metadata_utils::ValidationResult::TIME_UNIT_INVALID,
            metadata_utils::ValidateMetadata(metadata));

  metadata.set_time_unit(proto::DAY);
  EXPECT_EQ(metadata_utils::ValidationResult::VALIDATION_SUCCESS,
            metadata_utils::ValidateMetadata(metadata));
}

TEST_F(MetadataUtilsTest, MetadataFeatureValidation) {
  proto::Feature feature;
  EXPECT_EQ(metadata_utils::ValidationResult::SIGNAL_TYPE_INVALID,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_type(proto::SignalType::UNKNOWN_SIGNAL_TYPE);
  EXPECT_EQ(metadata_utils::ValidationResult::SIGNAL_TYPE_INVALID,
            metadata_utils::ValidateMetadataFeature(feature));

  // name not required for USER_ACTION.
  feature.set_type(proto::SignalType::USER_ACTION);
  EXPECT_EQ(metadata_utils::ValidationResult::FEATURE_NAME_HASH_NOT_FOUND,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_type(proto::SignalType::HISTOGRAM_ENUM);
  EXPECT_EQ(metadata_utils::ValidationResult::FEATURE_NAME_NOT_FOUND,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_type(proto::SignalType::HISTOGRAM_VALUE);
  EXPECT_EQ(metadata_utils::ValidationResult::FEATURE_NAME_NOT_FOUND,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_name("test name");
  EXPECT_EQ(metadata_utils::ValidationResult::FEATURE_NAME_HASH_NOT_FOUND,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_name_hash(123);
  EXPECT_EQ(metadata_utils::ValidationResult::FEATURE_AGGREGATION_NOT_FOUND,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_aggregation(proto::Aggregation::COUNT);
  EXPECT_EQ(metadata_utils::ValidationResult::FEATURE_BUCKET_COUNT_NOT_FOUND,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_bucket_count(456);
  EXPECT_EQ(metadata_utils::ValidationResult::FEATURE_TENSOR_LENGTH_NOT_FOUND,
            metadata_utils::ValidateMetadataFeature(feature));

  std::vector<proto::Aggregation> tensor_length_1 = {
      proto::Aggregation::COUNT,
      proto::Aggregation::COUNT_BOOLEAN,
      proto::Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT,
      proto::Aggregation::SUM,
      proto::Aggregation::SUM_BOOLEAN,
      proto::Aggregation::BUCKETED_SUM_BOOLEAN_TRUE_COUNT,
  };
  std::vector<proto::Aggregation> tensor_length_bucket_count = {
      proto::Aggregation::BUCKETED_COUNT,
      proto::Aggregation::BUCKETED_COUNT_BOOLEAN,
      proto::Aggregation::BUCKETED_CUMULATIVE_COUNT,
      proto::Aggregation::BUCKETED_SUM,
      proto::Aggregation::BUCKETED_SUM_BOOLEAN,
      proto::Aggregation::BUCKETED_CUMULATIVE_SUM,
  };

  for (auto aggregation : tensor_length_1) {
    feature.set_aggregation(aggregation);

    // If bucket count is 0, do not use for output, i.e. tensor_length should be
    // 0.
    feature.set_bucket_count(0);
    feature.set_tensor_length(1);
    EXPECT_EQ(metadata_utils::ValidationResult::FEATURE_TENSOR_LENGTH_INVALID,
              metadata_utils::ValidateMetadataFeature(feature));
    feature.set_tensor_length(0);
    EXPECT_EQ(metadata_utils::ValidationResult::VALIDATION_SUCCESS,
              metadata_utils::ValidateMetadataFeature(feature));

    // Tensor length should otherwise always be 1 for this aggregation type.
    feature.set_bucket_count(456);
    feature.set_tensor_length(10);
    EXPECT_EQ(metadata_utils::ValidationResult::FEATURE_TENSOR_LENGTH_INVALID,
              metadata_utils::ValidateMetadataFeature(feature));

    feature.set_bucket_count(456);
    feature.set_tensor_length(1);
    EXPECT_EQ(metadata_utils::ValidationResult::VALIDATION_SUCCESS,
              metadata_utils::ValidateMetadataFeature(feature));
  }

  for (auto aggregation : tensor_length_bucket_count) {
    feature.set_aggregation(aggregation);

    // If bucket count is 0, do not use for output, i.e. tensor_length should be
    // 0.
    feature.set_bucket_count(0);
    feature.set_tensor_length(1);
    EXPECT_EQ(metadata_utils::ValidationResult::FEATURE_TENSOR_LENGTH_INVALID,
              metadata_utils::ValidateMetadataFeature(feature));
    feature.set_tensor_length(0);
    EXPECT_EQ(metadata_utils::ValidationResult::VALIDATION_SUCCESS,
              metadata_utils::ValidateMetadataFeature(feature));

    // Tensor length should otherwise always be equal to bucket_count for this
    // aggregation type.
    feature.set_bucket_count(456);
    feature.set_tensor_length(1);
    EXPECT_EQ(metadata_utils::ValidationResult::FEATURE_TENSOR_LENGTH_INVALID,
              metadata_utils::ValidateMetadataFeature(feature));

    feature.set_bucket_count(456);
    feature.set_tensor_length(456);
    EXPECT_EQ(metadata_utils::ValidationResult::VALIDATION_SUCCESS,
              metadata_utils::ValidateMetadataFeature(feature));
  }
}

TEST_F(MetadataUtilsTest, ValidateMetadataAndFeatures) {
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::UNKNOWN_TIME_UNIT);
  EXPECT_EQ(metadata_utils::ValidationResult::TIME_UNIT_INVALID,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  metadata.set_time_unit(proto::DAY);
  EXPECT_EQ(metadata_utils::ValidationResult::VALIDATION_SUCCESS,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Verify adding a single features adds new requirements.
  auto* feature1 = metadata.add_features();
  EXPECT_EQ(metadata_utils::ValidationResult::SIGNAL_TYPE_INVALID,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Fully flesh out an example feature and verify validation starts working
  // again.
  feature1->set_type(proto::SignalType::USER_ACTION);
  feature1->set_name_hash(42);
  feature1->set_aggregation(proto::Aggregation::COUNT);
  feature1->set_bucket_count(1);
  feature1->set_tensor_length(1);
  EXPECT_EQ(metadata_utils::ValidationResult::VALIDATION_SUCCESS,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Verify adding another feature adds new requirements again.
  auto* feature2 = metadata.add_features();
  EXPECT_EQ(metadata_utils::ValidationResult::SIGNAL_TYPE_INVALID,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Fully flesh out the second feature and verify validation starts working
  // again.
  feature2->set_type(proto::SignalType::HISTOGRAM_VALUE);
  feature2->set_name("42");
  feature2->set_name_hash(42);
  feature2->set_aggregation(proto::Aggregation::BUCKETED_COUNT);
  feature2->set_bucket_count(2);
  feature2->set_tensor_length(2);
  EXPECT_EQ(metadata_utils::ValidationResult::VALIDATION_SUCCESS,
            metadata_utils::ValidateMetadataAndFeatures(metadata));
}

TEST_F(MetadataUtilsTest, ValidateSegementInfoMetadataAndFeatures) {
  proto::SegmentInfo segment_info;
  EXPECT_EQ(
      metadata_utils::ValidationResult::SEGMENT_ID_NOT_FOUND,
      metadata_utils::ValidateSegementInfoMetadataAndFeatures(segment_info));

  segment_info.set_segment_id(optimization_guide::proto::OptimizationTarget::
                                  OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  EXPECT_EQ(
      metadata_utils::ValidationResult::METADATA_NOT_FOUND,
      metadata_utils::ValidateSegementInfoMetadataAndFeatures(segment_info));

  auto* metadata = segment_info.mutable_model_metadata();
  metadata->set_time_unit(proto::DAY);
  EXPECT_EQ(
      metadata_utils::ValidationResult::VALIDATION_SUCCESS,
      metadata_utils::ValidateSegementInfoMetadataAndFeatures(segment_info));

  // Verify adding a single features adds new requirements.
  auto* feature1 = metadata->add_features();
  EXPECT_EQ(
      metadata_utils::ValidationResult::SIGNAL_TYPE_INVALID,
      metadata_utils::ValidateSegementInfoMetadataAndFeatures(segment_info));

  // Fully flesh out an example feature and verify validation starts working
  // again.
  feature1->set_type(proto::SignalType::USER_ACTION);
  feature1->set_name_hash(42);
  feature1->set_aggregation(proto::Aggregation::COUNT);
  feature1->set_bucket_count(1);
  feature1->set_tensor_length(1);
  EXPECT_EQ(
      metadata_utils::ValidationResult::VALIDATION_SUCCESS,
      metadata_utils::ValidateSegementInfoMetadataAndFeatures(segment_info));
}

TEST_F(MetadataUtilsTest, HasFreshResults) {
  proto::SegmentInfo segment_info;
  // No result.
  EXPECT_FALSE(metadata_utils::HasFreshResults(segment_info));

  auto* metadata = segment_info.mutable_model_metadata();
  metadata->set_result_time_to_live(1);
  metadata->set_time_unit(proto::DAY);

  // Stale results.
  auto* prediction_result = segment_info.mutable_prediction_result();
  base::Time result_time = base::Time::Now() - base::TimeDelta::FromDays(3);
  prediction_result->set_timestamp_us(
      result_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_FALSE(metadata_utils::HasFreshResults(segment_info));

  // Fresh results.
  result_time = base::Time::Now() - base::TimeDelta::FromHours(2);
  prediction_result->set_timestamp_us(
      result_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(metadata_utils::HasFreshResults(segment_info));
}

TEST_F(MetadataUtilsTest, HasExpiredOrUnavailableResult) {
  proto::SegmentInfo segment_info;
  auto* metadata = segment_info.mutable_model_metadata();
  metadata->set_result_time_to_live(7);
  metadata->set_time_unit(proto::TimeUnit::DAY);

  // No result.
  EXPECT_TRUE(metadata_utils::HasExpiredOrUnavailableResult(segment_info));

  // Unexpired result.
  auto* prediction_result = segment_info.mutable_prediction_result();
  base::Time result_time = base::Time::Now() - base::TimeDelta::FromDays(3);
  prediction_result->set_timestamp_us(
      result_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_FALSE(metadata_utils::HasExpiredOrUnavailableResult(segment_info));

  // Expired result.
  result_time = base::Time::Now() - base::TimeDelta::FromDays(30);
  prediction_result->set_timestamp_us(
      result_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_TRUE(metadata_utils::HasExpiredOrUnavailableResult(segment_info));
}

TEST_F(MetadataUtilsTest, GetTimeUnit) {
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::TimeUnit::DAY);
  EXPECT_EQ(base::TimeDelta::FromDays(1),
            metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::HOUR);
  EXPECT_EQ(base::TimeDelta::FromHours(1),
            metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::MINUTE);
  EXPECT_EQ(base::TimeDelta::FromMinutes(1),
            metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::SECOND);
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::WEEK);
  EXPECT_EQ(base::TimeDelta::FromDays(7),
            metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::MONTH);
  EXPECT_EQ(base::TimeDelta::FromDays(30),
            metadata_utils::GetTimeUnit(metadata));

  metadata.set_time_unit(proto::TimeUnit::YEAR);
  EXPECT_EQ(base::TimeDelta::FromDays(365),
            metadata_utils::GetTimeUnit(metadata));
}

TEST_F(MetadataUtilsTest, GetNameHashForFeature) {
  proto::Feature feature;
  EXPECT_FALSE(metadata_utils::GetNameHashForFeature(feature).has_value());
  feature.set_name_hash(42);
  auto name_hash = metadata_utils::GetNameHashForFeature(feature);
  EXPECT_TRUE(name_hash.has_value());
  EXPECT_EQ(42u, name_hash.value());
}

TEST_F(MetadataUtilsTest, GetSignalTypeForFeature) {
  proto::Feature feature;
  EXPECT_EQ(proto::SignalType::UNKNOWN_SIGNAL_TYPE,
            metadata_utils::GetSignalTypeForFeature(feature));

  feature.set_type(proto::SignalType::UNKNOWN_SIGNAL_TYPE);
  EXPECT_EQ(proto::SignalType::UNKNOWN_SIGNAL_TYPE,
            metadata_utils::GetSignalTypeForFeature(feature));

  feature.set_type(proto::SignalType::USER_ACTION);
  EXPECT_EQ(proto::SignalType::USER_ACTION,
            metadata_utils::GetSignalTypeForFeature(feature));

  feature.set_type(proto::SignalType::HISTOGRAM_ENUM);
  EXPECT_EQ(proto::SignalType::HISTOGRAM_ENUM,
            metadata_utils::GetSignalTypeForFeature(feature));

  feature.set_type(proto::SignalType::HISTOGRAM_VALUE);
  EXPECT_EQ(proto::SignalType::HISTOGRAM_VALUE,
            metadata_utils::GetSignalTypeForFeature(feature));
}

TEST_F(MetadataUtilsTest, SignalTypeToSignalKind) {
  EXPECT_EQ(
      SignalKey::Kind::USER_ACTION,
      metadata_utils::SignalTypeToSignalKind(proto::SignalType::USER_ACTION));
  EXPECT_EQ(SignalKey::Kind::HISTOGRAM_ENUM,
            metadata_utils::SignalTypeToSignalKind(
                proto::SignalType::HISTOGRAM_ENUM));
  EXPECT_EQ(SignalKey::Kind::HISTOGRAM_VALUE,
            metadata_utils::SignalTypeToSignalKind(
                proto::SignalType::HISTOGRAM_VALUE));
  EXPECT_EQ(SignalKey::Kind::UNKNOWN,
            metadata_utils::SignalTypeToSignalKind(
                proto::SignalType::UNKNOWN_SIGNAL_TYPE));
}

}  // namespace segmentation_platform
