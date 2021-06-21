// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/metadata_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class MetadataUtilsTest : public testing::Test {
 public:
  ~MetadataUtilsTest() override = default;
};

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

}  // namespace segmentation_platform
