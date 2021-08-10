// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/metadata_utils.h"

#include "base/metrics/metrics_hashes.h"
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
  EXPECT_EQ(metadata_utils::ValidationResult::kSegmentIDNotFound,
            metadata_utils::ValidateSegmentInfo(segment_info));

  segment_info.set_segment_id(optimization_guide::proto::OptimizationTarget::
                                  OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  EXPECT_EQ(metadata_utils::ValidationResult::kMetadataNotFound,
            metadata_utils::ValidateSegmentInfo(segment_info));

  // The rest of this test verifies that at least some metadata is verified.
  segment_info.mutable_model_metadata()->set_time_unit(
      proto::UNKNOWN_TIME_UNIT);
  EXPECT_EQ(metadata_utils::ValidationResult::kTimeUnitInvald,
            metadata_utils::ValidateSegmentInfo(segment_info));

  segment_info.mutable_model_metadata()->set_time_unit(proto::DAY);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateSegmentInfo(segment_info));
}

TEST_F(MetadataUtilsTest, DefaultMetadataIsInvalid) {
  proto::SegmentationModelMetadata empty;

  EXPECT_EQ(metadata_utils::ValidationResult::kTimeUnitInvald,
            metadata_utils::ValidateMetadata(empty));
}

TEST_F(MetadataUtilsTest, MetadataValidation) {
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::UNKNOWN_TIME_UNIT);
  EXPECT_EQ(metadata_utils::ValidationResult::kTimeUnitInvald,
            metadata_utils::ValidateMetadata(metadata));

  metadata.set_time_unit(proto::DAY);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadata(metadata));
}

TEST_F(MetadataUtilsTest, MetadataFeatureValidation) {
  proto::Feature feature;
  EXPECT_EQ(metadata_utils::ValidationResult::kSignalTypeInvalid,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_type(proto::SignalType::UNKNOWN_SIGNAL_TYPE);
  EXPECT_EQ(metadata_utils::ValidationResult::kSignalTypeInvalid,
            metadata_utils::ValidateMetadataFeature(feature));

  // name not required for USER_ACTION.
  feature.set_type(proto::SignalType::USER_ACTION);
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureNameHashNotFound,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_type(proto::SignalType::HISTOGRAM_ENUM);
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureNameNotFound,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_type(proto::SignalType::HISTOGRAM_VALUE);
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureNameNotFound,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_name("test name");
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureNameHashNotFound,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_name_hash(base::HashMetricName("not the correct name"));
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureNameHashDoesNotMatchName,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_name_hash(base::HashMetricName("test name"));
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureAggregationNotFound,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_aggregation(proto::Aggregation::COUNT);
  // No bucket_count or tensor_length is valid.
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataFeature(feature));

  feature.set_bucket_count(456);
  // Aggregation=COUNT requires tensor length = 1.
  EXPECT_EQ(metadata_utils::ValidationResult::kFeatureTensorLengthInvalid,
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
    EXPECT_EQ(metadata_utils::ValidationResult::kFeatureTensorLengthInvalid,
              metadata_utils::ValidateMetadataFeature(feature));
    feature.set_tensor_length(0);
    EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
              metadata_utils::ValidateMetadataFeature(feature));

    // Tensor length should otherwise always be 1 for this aggregation type.
    feature.set_bucket_count(456);
    feature.set_tensor_length(10);
    EXPECT_EQ(metadata_utils::ValidationResult::kFeatureTensorLengthInvalid,
              metadata_utils::ValidateMetadataFeature(feature));

    feature.set_bucket_count(456);
    feature.set_tensor_length(1);
    EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
              metadata_utils::ValidateMetadataFeature(feature));
  }

  for (auto aggregation : tensor_length_bucket_count) {
    feature.set_aggregation(aggregation);

    // If bucket count is 0, do not use for output, i.e. tensor_length should be
    // 0.
    feature.set_bucket_count(0);
    feature.set_tensor_length(1);
    EXPECT_EQ(metadata_utils::ValidationResult::kFeatureTensorLengthInvalid,
              metadata_utils::ValidateMetadataFeature(feature));
    feature.set_tensor_length(0);
    EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
              metadata_utils::ValidateMetadataFeature(feature));

    // Tensor length should otherwise always be equal to bucket_count for this
    // aggregation type.
    feature.set_bucket_count(456);
    feature.set_tensor_length(1);
    EXPECT_EQ(metadata_utils::ValidationResult::kFeatureTensorLengthInvalid,
              metadata_utils::ValidateMetadataFeature(feature));

    feature.set_bucket_count(456);
    feature.set_tensor_length(456);
    EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
              metadata_utils::ValidateMetadataFeature(feature));
  }
}

TEST_F(MetadataUtilsTest, ValidateMetadataAndFeatures) {
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::UNKNOWN_TIME_UNIT);
  EXPECT_EQ(metadata_utils::ValidationResult::kTimeUnitInvald,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  metadata.set_time_unit(proto::DAY);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Verify adding a single features adds new requirements.
  auto* feature1 = metadata.add_features();
  EXPECT_EQ(metadata_utils::ValidationResult::kSignalTypeInvalid,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Fully flesh out an example feature and verify validation starts working
  // again.
  feature1->set_type(proto::SignalType::USER_ACTION);
  feature1->set_name_hash(base::HashMetricName("some user action"));
  feature1->set_aggregation(proto::Aggregation::COUNT);
  feature1->set_bucket_count(1);
  feature1->set_tensor_length(1);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Verify adding another feature adds new requirements again.
  auto* feature2 = metadata.add_features();
  EXPECT_EQ(metadata_utils::ValidationResult::kSignalTypeInvalid,
            metadata_utils::ValidateMetadataAndFeatures(metadata));

  // Fully flesh out the second feature and verify validation starts working
  // again.
  feature2->set_type(proto::SignalType::HISTOGRAM_VALUE);
  feature2->set_name("some histogram");
  feature2->set_name_hash(base::HashMetricName("some histogram"));
  feature2->set_aggregation(proto::Aggregation::BUCKETED_COUNT);
  feature2->set_bucket_count(2);
  feature2->set_tensor_length(2);
  EXPECT_EQ(metadata_utils::ValidationResult::kValidationSuccess,
            metadata_utils::ValidateMetadataAndFeatures(metadata));
}

TEST_F(MetadataUtilsTest, ValidateSegementInfoMetadataAndFeatures) {
  proto::SegmentInfo segment_info;
  EXPECT_EQ(
      metadata_utils::ValidationResult::kSegmentIDNotFound,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));

  segment_info.set_segment_id(optimization_guide::proto::OptimizationTarget::
                                  OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  EXPECT_EQ(
      metadata_utils::ValidationResult::kMetadataNotFound,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));

  auto* metadata = segment_info.mutable_model_metadata();
  metadata->set_time_unit(proto::DAY);
  EXPECT_EQ(
      metadata_utils::ValidationResult::kValidationSuccess,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));

  // Verify adding a single features adds new requirements.
  auto* feature1 = metadata->add_features();
  EXPECT_EQ(
      metadata_utils::ValidationResult::kSignalTypeInvalid,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));

  // Fully flesh out an example feature and verify validation starts working
  // again.
  feature1->set_type(proto::SignalType::USER_ACTION);
  feature1->set_name_hash(base::HashMetricName("some user action"));
  feature1->set_aggregation(proto::Aggregation::COUNT);
  feature1->set_bucket_count(1);
  feature1->set_tensor_length(1);
  EXPECT_EQ(
      metadata_utils::ValidationResult::kValidationSuccess,
      metadata_utils::ValidateSegmentInfoMetadataAndFeatures(segment_info));
}

TEST_F(MetadataUtilsTest, SetFeatureNameHashesFromName) {
  // No crashes should happen if there are no features.
  proto::SegmentationModelMetadata empty;
  metadata_utils::SetFeatureNameHashesFromName(&empty);

  // Ensure that the name hash is overwritten.
  proto::SegmentationModelMetadata one_feature_both_set;
  auto* feature1 = one_feature_both_set.add_features();
  feature1->set_name("both set");
  feature1->set_name_hash(base::HashMetricName("both set"));
  metadata_utils::SetFeatureNameHashesFromName(&one_feature_both_set);
  EXPECT_EQ(1, one_feature_both_set.features_size());
  EXPECT_EQ("both set", one_feature_both_set.features(0).name());
  EXPECT_EQ(base::HashMetricName("both set"),
            one_feature_both_set.features(0).name_hash());

  // Ensure that the name hash is overwritten if it is incorrect.
  proto::SegmentationModelMetadata one_feature_both_set_hash_incorrect;
  auto* feature2 = one_feature_both_set_hash_incorrect.add_features();
  feature2->set_name("both set");
  feature2->set_name_hash(base::HashMetricName("INCORRECT NAME HASH"));
  metadata_utils::SetFeatureNameHashesFromName(
      &one_feature_both_set_hash_incorrect);
  EXPECT_EQ(1, one_feature_both_set_hash_incorrect.features_size());
  EXPECT_EQ("both set", one_feature_both_set_hash_incorrect.features(0).name());
  EXPECT_EQ(base::HashMetricName("both set"),
            one_feature_both_set_hash_incorrect.features(0).name_hash());

  // Ensure that the name hash is set from the name.
  proto::SegmentationModelMetadata one_feature_name_set;
  auto* feature3 = one_feature_name_set.add_features();
  feature3->set_name("only name set");
  metadata_utils::SetFeatureNameHashesFromName(&one_feature_name_set);
  EXPECT_EQ(1, one_feature_name_set.features_size());
  EXPECT_EQ("only name set", one_feature_name_set.features(0).name());
  EXPECT_EQ(base::HashMetricName("only name set"),
            one_feature_name_set.features(0).name_hash());

  // Name hash should be overwritten with the hash of the empty string in the
  // case of only the name hash having been set.
  proto::SegmentationModelMetadata one_feature_name_hash_set;
  auto* feature4 = one_feature_name_hash_set.add_features();
  feature4->set_name_hash(base::HashMetricName("only name hash set"));
  metadata_utils::SetFeatureNameHashesFromName(&one_feature_name_hash_set);
  EXPECT_EQ(1, one_feature_name_hash_set.features_size());
  EXPECT_EQ("", one_feature_name_hash_set.features(0).name());
  EXPECT_EQ(base::HashMetricName(""),
            one_feature_name_hash_set.features(0).name_hash());

  // When neither name nor name hash is set, we should still overwrite the name
  // hash with the hash of the empty string.
  proto::SegmentationModelMetadata one_feature_nothing_set;
  // Add feature and set a different field to ensure it is added.
  auto* feature5 = one_feature_nothing_set.add_features();
  feature5->set_type(proto::SignalType::USER_ACTION);
  metadata_utils::SetFeatureNameHashesFromName(&one_feature_nothing_set);
  EXPECT_EQ(1, one_feature_nothing_set.features_size());
  EXPECT_EQ("", one_feature_nothing_set.features(0).name());
  EXPECT_EQ(base::HashMetricName(""),
            one_feature_nothing_set.features(0).name_hash());

  // Ensure that the name hash is set for all features.
  proto::SegmentationModelMetadata multiple_features;
  auto* multifeature1 = multiple_features.add_features();
  multifeature1->set_name("first multi");
  auto* multifeature2 = multiple_features.add_features();
  multifeature2->set_name("second multi");
  metadata_utils::SetFeatureNameHashesFromName(&multiple_features);
  EXPECT_EQ(2, multiple_features.features_size());
  EXPECT_EQ("first multi", multiple_features.features(0).name());
  EXPECT_EQ(base::HashMetricName("first multi"),
            multiple_features.features(0).name_hash());
  EXPECT_EQ("second multi", multiple_features.features(1).name());
  EXPECT_EQ(base::HashMetricName("second multi"),
            multiple_features.features(1).name_hash());
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
