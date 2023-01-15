// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/config_parser.h"
#include <memory>

#include "components/segmentation_platform/public/config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

bool operator==(const std::unique_ptr<Config::SegmentMetadata>& a,
                const std::unique_ptr<Config::SegmentMetadata>& b) {
  return *a == *b;
}

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
      "16" : {"segment_uma_name" : "LowEngagement"},
      "5" : {"segment_uma_name" : "HighEngagement"},
      "6" : {"segment_uma_name" : "MediumEngagement"}
    },
    "segment_selection_ttl_days": 10
  })";
  auto config1 = ParseConfigFromString(kValidConfig1);
  ASSERT_TRUE(config1);
  EXPECT_EQ(config1->segmentation_key, "test_key");
  EXPECT_EQ(config1->segmentation_uma_name, "TestKey");
  base::flat_map<proto::SegmentId, std::unique_ptr<Config::SegmentMetadata>>
      expected1;
  expected1.insert(
      {proto::SegmentId::
           OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT,
       std::make_unique<Config::SegmentMetadata>("LowEngagement")});
  expected1.insert(
      {proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
       std::make_unique<Config::SegmentMetadata>("HighEngagement")});
  expected1.insert(
      {proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE,
       std::make_unique<Config::SegmentMetadata>("MediumEngagement")});
  EXPECT_EQ(config1->segments, expected1);
  EXPECT_EQ(config1->segment_selection_ttl, base::Days(10));
  EXPECT_EQ(config1->unknown_selection_ttl, base::Days(0));

  constexpr char kValidConfig2[] = R"({
      "segmentation_key": "test_key",
      "segmentation_uma_name": "TestKey",
      "segments": {
        "5" : {"segment_uma_name" : "FeedUser"}
      },
      "segment_selection_ttl_days": 10,
      "unknown_segment_selection_ttl_days": 14
  })";
  auto config2 = ParseConfigFromString(kValidConfig2);
  ASSERT_TRUE(config2);
  EXPECT_EQ(config2->segmentation_key, "test_key");
  EXPECT_EQ(config2->segmentation_uma_name, "TestKey");
  base::flat_map<proto::SegmentId, std::unique_ptr<Config::SegmentMetadata>>
      expected2;
  expected2.insert({proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
                    std::make_unique<Config::SegmentMetadata>("FeedUser")});
  EXPECT_EQ(config2->segments, expected2);
  EXPECT_EQ(config2->segment_selection_ttl, base::Days(10));
  EXPECT_EQ(config2->unknown_selection_ttl, base::Days(14));
}

}  // namespace segmentation_platform
