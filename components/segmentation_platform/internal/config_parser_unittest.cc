// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/config_parser.h"

#include "components/segmentation_platform/public/config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

TEST(ConfigParserTest, ParseInvalidConfig) {
  constexpr char kInvalidJson[] =
      R"({"segmentation_key":"test_key, "uma_name":})";
  EXPECT_FALSE(ParseConfigFromString(kInvalidJson));

  constexpr char kMissingFields[] = R"({"segmentation_key":"test_key"})";
  EXPECT_FALSE(ParseConfigFromString(kMissingFields));
}

TEST(ConfigParserTest, ParseValidConfig) {
  constexpr char kValidConfig1[] = R"({
    "segmentation_key": "test_key",
    "segmentation_uma_name": "TestKey",
    "segments": {
      "2" : {"segment_uma_name" : "LowEngagement"},
      "7" : {"segment_uma_name" : "HighEngagement"},
      "9" : {"segment_uma_name" : "MediumEngagement"}
    },
    "segment_selection_ttl_days": 10
  })";
  auto config1 = ParseConfigFromString(kValidConfig1);
  ASSERT_TRUE(config1);
  EXPECT_EQ(config1->segmentation_key, "test_key");
  EXPECT_EQ(config1->segmentation_uma_name, "TestKey");
  std::unordered_map<proto::SegmentId, Config::SegmentMetadata> expected1{
      {proto::SegmentId::OPTIMIZATION_TARGET_MODEL_VALIDATION,
       Config::SegmentMetadata{"HighEngagement"}},
      {proto::SegmentId::
           OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS,
       Config::SegmentMetadata{"MediumEngagement"}},
      {proto::SegmentId::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
       Config::SegmentMetadata{"LowEngagement"}}};
  EXPECT_EQ(config1->segments, expected1);
  EXPECT_EQ(config1->segment_selection_ttl, base::Days(10));
  EXPECT_EQ(config1->unknown_selection_ttl, base::Days(0));

  constexpr char kValidConfig2[] = R"({
      "segmentation_key": "test_key",
      "segmentation_uma_name": "TestKey",
      "segments": {
        "9" : {"segment_uma_name" : "FeedUser"}
      },
      "segment_selection_ttl_days": 10,
      "unknown_segment_selection_ttl_days": 14
  })";
  auto config2 = ParseConfigFromString(kValidConfig2);
  ASSERT_TRUE(config2);
  EXPECT_EQ(config2->segmentation_key, "test_key");
  EXPECT_EQ(config2->segmentation_uma_name, "TestKey");
  std::unordered_map<proto::SegmentId, Config::SegmentMetadata> expected2{
      {proto::SegmentId::
           OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS,
       Config::SegmentMetadata{"FeedUser"}}};
  EXPECT_EQ(config2->segments, expected2);
  EXPECT_EQ(config2->segment_selection_ttl, base::Days(10));
  EXPECT_EQ(config2->unknown_selection_ttl, base::Days(14));
}

}  // namespace segmentation_platform
