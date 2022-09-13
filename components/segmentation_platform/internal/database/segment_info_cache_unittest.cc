// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/segment_info_cache.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using InitStatus = leveldb_proto::Enums::InitStatus;

namespace segmentation_platform {

namespace {

// Test Ids.
const proto::SegmentId kSegmentId =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;

const proto::SegmentId kSegmentId2 =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER;

base::flat_set<SegmentId> ids_missing_from_cache;

proto::SegmentInfo CreateSegment(SegmentId segment_id) {
  proto::SegmentInfo info;
  info.set_segment_id(segment_id);
  return info;
}
}  // namespace

class SegmentInfoCacheTest : public testing::Test {
 public:
  SegmentInfoCacheTest() = default;
  ~SegmentInfoCacheTest() override = default;

 protected:
  void SetUpCache(bool cache_enabled) {
    ids_missing_from_cache.clear();
    DCHECK(!segment_info_cache_);
    segment_info_cache_ = std::make_unique<SegmentInfoCache>(cache_enabled);
  }

  void TearDown() override { segment_info_cache_.reset(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SegmentInfoCache> segment_info_cache_;
};

TEST_F(SegmentInfoCacheTest, GetSegmentInfoFromEmptyCache) {
  SetUpCache(true /*cache_enabled*/);

  auto segment_info_ = segment_info_cache_->GetSegmentInfo(kSegmentId);
  EXPECT_FALSE(segment_info_.has_value());
}

TEST_F(SegmentInfoCacheTest, GetSegmentInfoFromCache) {
  SetUpCache(true /*cache_enabled*/);
  segment_info_cache_->UpdateSegmentInfo(kSegmentId, CreateSegment(kSegmentId));
  auto segment_info_ = segment_info_cache_->GetSegmentInfo(kSegmentId);
  EXPECT_TRUE(segment_info_.has_value());
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());
}

TEST_F(SegmentInfoCacheTest, GetSegmentInfoForSegmentsFromCache) {
  SetUpCache(true /*cache_enabled*/);
  segment_info_cache_->UpdateSegmentInfo(kSegmentId, CreateSegment(kSegmentId));
  auto segments_so_far = segment_info_cache_->GetSegmentInfoForSegments(
      {kSegmentId, kSegmentId2}, ids_missing_from_cache);
  EXPECT_EQ(1u, segments_so_far.get()->size());
  EXPECT_EQ(kSegmentId, segments_so_far.get()->at(0).first);
  EXPECT_THAT(ids_missing_from_cache, testing::ElementsAre(kSegmentId2));

  ids_missing_from_cache.clear();
  segment_info_cache_->UpdateSegmentInfo(kSegmentId2,
                                         CreateSegment(kSegmentId2));
  segments_so_far = segment_info_cache_->GetSegmentInfoForSegments(
      {kSegmentId, kSegmentId2}, ids_missing_from_cache);
  EXPECT_EQ(2u, segments_so_far.get()->size());
  EXPECT_EQ(kSegmentId, segments_so_far.get()->at(0).first);
  EXPECT_EQ(kSegmentId2, segments_so_far.get()->at(1).first);
  EXPECT_TRUE(ids_missing_from_cache.empty());
}

TEST_F(SegmentInfoCacheTest, UpdateSegmentInfo) {
  SetUpCache(true /*cache_enabled*/);

  proto::SegmentInfo created_segment_info = CreateSegment(kSegmentId);
  segment_info_cache_->UpdateSegmentInfo(kSegmentId, created_segment_info);

  auto segment_info_ = segment_info_cache_->GetSegmentInfo(kSegmentId);

  EXPECT_TRUE(segment_info_.has_value());
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());

  // Update model_version of segment_info
  created_segment_info.set_model_version(4);
  segment_info_cache_->UpdateSegmentInfo(kSegmentId, created_segment_info);

  segment_info_ = segment_info_cache_->GetSegmentInfo(kSegmentId);
  EXPECT_TRUE(segment_info_.has_value());
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());
  EXPECT_EQ(4, segment_info_->model_version());
}

TEST_F(SegmentInfoCacheTest, GetOrUpdateSegmentInfoWhenCacheDisabled) {
  SetUpCache(false /*cache_enabled*/);
  segment_info_cache_->UpdateSegmentInfo(kSegmentId, CreateSegment(kSegmentId));
  auto segment_info_ = segment_info_cache_->GetSegmentInfo(kSegmentId);
  EXPECT_FALSE(segment_info_.has_value());
  auto segments_so_far = segment_info_cache_->GetSegmentInfoForSegments(
      {kSegmentId}, ids_missing_from_cache);
  EXPECT_TRUE(segments_so_far.get()->empty());
  EXPECT_THAT(ids_missing_from_cache, testing::ElementsAre(kSegmentId));
}

}  // namespace segmentation_platform