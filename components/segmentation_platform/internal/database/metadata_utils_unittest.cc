// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/metadata_utils.h"

#include "components/optimization_guide/proto/models.pb.h"
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

TEST_F(MetadataUtilsTest, HasFreshResults) {
  proto::SegmentInfo segment_info;
  // No result.
  EXPECT_FALSE(metadata_utils::HasFreshResults(segment_info));

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

TEST_F(MetadataUtilsTest, GetNameHashForFeature) {
  proto::Feature feature;
  proto::UserActionFeature* user_action = feature.mutable_user_action();
  user_action->set_user_action_hash(1);
  auto name_hash = metadata_utils::GetNameHashForFeature(feature);
  EXPECT_EQ(1u, name_hash.value());
  feature.clear_user_action();

  proto::HistogramEnumFeature* histogram_enum =
      feature.mutable_histogram_enum();
  histogram_enum->set_name_hash(2);
  name_hash = metadata_utils::GetNameHashForFeature(feature);
  EXPECT_EQ(2u, name_hash.value());
  feature.clear_histogram_enum();

  proto::HistogramValueFeature* histogram_value =
      feature.mutable_histogram_value();
  histogram_value->set_name_hash(3);
  name_hash = metadata_utils::GetNameHashForFeature(feature);
  EXPECT_EQ(3u, name_hash.value());
}

TEST_F(MetadataUtilsTest, GetSignalTypeForFeature) {
  proto::Feature feature;
  proto::UserActionFeature* user_action = feature.mutable_user_action();
  user_action->set_user_action_hash(1);
  EXPECT_EQ(proto::SignalType::USER_ACTION,
            metadata_utils::GetSignalTypeForFeature(feature));
  feature.clear_user_action();

  proto::HistogramEnumFeature* histogram_enum =
      feature.mutable_histogram_enum();
  histogram_enum->set_name_hash(2);
  EXPECT_EQ(proto::SignalType::HISTOGRAM_ENUM,
            metadata_utils::GetSignalTypeForFeature(feature));
  feature.clear_histogram_enum();

  proto::HistogramValueFeature* histogram_value =
      feature.mutable_histogram_value();
  histogram_value->set_name_hash(3);
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
}

}  // namespace segmentation_platform
