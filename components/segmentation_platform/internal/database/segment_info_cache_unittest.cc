// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/segment_info_cache.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

// Test Ids.
const proto::SegmentId kSegmentId =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;

const proto::SegmentId kSegmentId2 =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER;

const proto::SegmentId kSegmentId3 =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER;

const ModelSource kDefaultModelSource = ModelSource::DEFAULT_MODEL_SOURCE;
const ModelSource kServerModelSource = ModelSource::SERVER_MODEL_SOURCE;
const ModelSource kUnknownModelSource = ModelSource::UNKNOWN_MODEL_SOURCE;

proto::SegmentInfo CreateSegment(SegmentId segment_id,
                                 ModelSource model_source) {
  proto::SegmentInfo info;
  info.set_segment_id(segment_id);
  info.set_model_source(model_source);
  return info;
}
}  // namespace

class SegmentInfoCacheTest : public testing::Test {
 public:
  SegmentInfoCacheTest() = default;
  ~SegmentInfoCacheTest() override = default;

 protected:
  void SetUp() override {
    DCHECK(!segment_info_cache_);
    segment_info_cache_ = std::make_unique<SegmentInfoCache>();
  }

  void TearDown() override { segment_info_cache_.reset(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SegmentInfoCache> segment_info_cache_;
};

TEST_F(SegmentInfoCacheTest, GetSegmentInfoFromEmptyCache) {
  // Server Model
  const auto* segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kServerModelSource);
  EXPECT_EQ(nullptr, segment_info_);
  // Default model
  segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kDefaultModelSource);
  EXPECT_EQ(nullptr, segment_info_);
}

TEST_F(SegmentInfoCacheTest, GetSegmentInfoFromCache) {
  segment_info_cache_->UpdateSegmentInfo(
      kSegmentId, kServerModelSource,
      CreateSegment(kSegmentId, kServerModelSource));
  const auto* segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kServerModelSource);
  EXPECT_TRUE(segment_info_);
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());

  // Calling GetSegmentInfo method again.
  segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kServerModelSource);
  EXPECT_TRUE(segment_info_);
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());

  // Calling GetSegmentInfo method again for default model.
  segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kDefaultModelSource);
  EXPECT_FALSE(segment_info_);

  // Creating SegmentInfo for default model and querying it.
  segment_info_cache_->UpdateSegmentInfo(
      kSegmentId, kDefaultModelSource,
      CreateSegment(kSegmentId, kDefaultModelSource));
  segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kDefaultModelSource);
  EXPECT_TRUE(segment_info_);
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());
  EXPECT_EQ(kDefaultModelSource, segment_info_->model_source());
}

TEST_F(SegmentInfoCacheTest, GetSegmentInfoForSegmentsFromCache) {
  // Updating SegmentInfo for 'kSegmentId' and calling
  // GetSegmentInfoForSegments with superset of segment ids.
  segment_info_cache_->UpdateSegmentInfo(
      kSegmentId, kServerModelSource,
      CreateSegment(kSegmentId, kServerModelSource));
  auto segments_found = segment_info_cache_->GetSegmentInfoForSegments(
      {kSegmentId, kSegmentId2}, kServerModelSource);
  EXPECT_EQ(1u, segments_found.get()->size());
  EXPECT_EQ(kSegmentId, segments_found.get()->at(0).first);

  // Creating default model segment for 'kSegmentId2' and calling
  // GetSegmentInfoForSegments with default and server model source
  // with all segment ids.
  segment_info_cache_->UpdateSegmentInfo(
      kSegmentId2, kDefaultModelSource,
      CreateSegment(kSegmentId2, kDefaultModelSource));
  segments_found = segment_info_cache_->GetSegmentInfoForSegments(
      {kSegmentId, kSegmentId2}, kServerModelSource);
  EXPECT_EQ(1u, segments_found.get()->size());
  EXPECT_EQ(kSegmentId, segments_found.get()->at(0).first);

  segments_found = segment_info_cache_->GetSegmentInfoForSegments(
      {kSegmentId, kSegmentId2}, kDefaultModelSource);
  EXPECT_EQ(1u, segments_found.get()->size());
  EXPECT_EQ(kSegmentId2, segments_found.get()->at(0).first);

  // Updating SegmentInfo for 'kSegmentId2' and calling
  // GetSegmentInfoForSegments with all segment ids.
  segment_info_cache_->UpdateSegmentInfo(
      kSegmentId2, kServerModelSource,
      CreateSegment(kSegmentId2, kServerModelSource));
  segments_found = segment_info_cache_->GetSegmentInfoForSegments(
      {kSegmentId, kSegmentId2}, kServerModelSource);
  EXPECT_EQ(2u, segments_found.get()->size());
  EXPECT_EQ(kSegmentId, segments_found.get()->at(0).first);
  EXPECT_EQ(kSegmentId2, segments_found.get()->at(1).first);

  // Updating std::nullopt for 'kSegmentId2' and calling
  // GetSegmentInfoForSegments with all segment ids.
  segment_info_cache_->UpdateSegmentInfo(kSegmentId2, kServerModelSource,
                                         std::nullopt);
  segments_found = segment_info_cache_->GetSegmentInfoForSegments(
      {kSegmentId, kSegmentId2}, kServerModelSource);
  EXPECT_EQ(1u, segments_found.get()->size());
  EXPECT_EQ(kSegmentId, segments_found.get()->at(0).first);
}

TEST_F(SegmentInfoCacheTest, GetSegmentInfoForBothModelsFromCache) {
  // Updating SegmentInfo for 'kSegmentId' and calling
  // GetSegmentInfoForBothModels for both segment ids.
  segment_info_cache_->UpdateSegmentInfo(
      kSegmentId, kServerModelSource,
      CreateSegment(kSegmentId, kServerModelSource));
  auto segments_found = segment_info_cache_->GetSegmentInfoForBothModels(
      {kSegmentId, kSegmentId2});
  EXPECT_EQ(1u, segments_found.get()->size());
  EXPECT_EQ(kSegmentId, segments_found.get()->at(0).first);

  // Creating default model segment for 'kSegmentId' and calling
  // GetSegmentInfoForBothModels with default and server model source.
  segment_info_cache_->UpdateSegmentInfo(
      kSegmentId, kDefaultModelSource,
      CreateSegment(kSegmentId, kDefaultModelSource));
  segments_found =
      segment_info_cache_->GetSegmentInfoForBothModels({kSegmentId});
  EXPECT_EQ(2u, segments_found.get()->size());

  EXPECT_EQ(kSegmentId, segments_found.get()->at(0).first);
  EXPECT_EQ(kServerModelSource,
            segments_found.get()->at(0).second->model_source());

  EXPECT_EQ(kSegmentId, segments_found.get()->at(1).first);
  EXPECT_EQ(kDefaultModelSource,
            segments_found.get()->at(1).second->model_source());

  // Updating SegmentInfo for 'kSegmentId2' and calling
  // GetSegmentInfoForBothModels with both segment ids.
  segment_info_cache_->UpdateSegmentInfo(
      kSegmentId2, kDefaultModelSource,
      CreateSegment(kSegmentId2, kDefaultModelSource));
  segments_found = segment_info_cache_->GetSegmentInfoForBothModels(
      {kSegmentId, kSegmentId2});
  EXPECT_EQ(3u, segments_found.get()->size());
  EXPECT_EQ(kSegmentId, segments_found.get()->at(0).first);
  EXPECT_EQ(kServerModelSource,
            segments_found.get()->at(0).second->model_source());

  EXPECT_EQ(kSegmentId, segments_found.get()->at(1).first);
  EXPECT_EQ(kDefaultModelSource,
            segments_found.get()->at(1).second->model_source());

  EXPECT_EQ(kSegmentId2, segments_found.get()->at(2).first);
  EXPECT_EQ(kDefaultModelSource,
            segments_found.get()->at(2).second->model_source());

  // Updating SegmentInfo for 'kSegmentId2' with std::nullopt and calling
  // GetSegmentInfoForBothModels with both segment ids.
  segment_info_cache_->UpdateSegmentInfo(kSegmentId2, kDefaultModelSource,
                                         std::nullopt);
  segments_found = segment_info_cache_->GetSegmentInfoForBothModels(
      {kSegmentId, kSegmentId2});
  EXPECT_EQ(2u, segments_found.get()->size());
  EXPECT_EQ(kSegmentId, segments_found.get()->at(0).first);
  EXPECT_EQ(kServerModelSource,
            segments_found.get()->at(0).second->model_source());

  EXPECT_EQ(kSegmentId, segments_found.get()->at(1).first);
  EXPECT_EQ(kDefaultModelSource,
            segments_found.get()->at(1).second->model_source());

  // Calling GetSegmentInfoForBothModels for `segment_id` not present in cache
  // for both model sources.
  segments_found =
      segment_info_cache_->GetSegmentInfoForBothModels({kSegmentId3});
  EXPECT_EQ(0u, segments_found.get()->size());
}

TEST_F(SegmentInfoCacheTest, UpdateSegmentInfo) {
  // Server Model
  proto::SegmentInfo created_segment_info =
      CreateSegment(kSegmentId, kServerModelSource);
  segment_info_cache_->UpdateSegmentInfo(kSegmentId, kServerModelSource,
                                         created_segment_info);

  const auto* segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kServerModelSource);
  EXPECT_TRUE(segment_info_);
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());
  EXPECT_EQ(0, segment_info_->model_version());

  // Update model_version of segment_info
  created_segment_info.set_model_version(4);
  segment_info_cache_->UpdateSegmentInfo(kSegmentId, kServerModelSource,
                                         created_segment_info);

  segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kServerModelSource);
  EXPECT_TRUE(segment_info_);
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());
  EXPECT_EQ(4, segment_info_->model_version());

  // Default model
  created_segment_info = CreateSegment(kSegmentId, kDefaultModelSource);
  segment_info_cache_->UpdateSegmentInfo(kSegmentId, kDefaultModelSource,
                                         created_segment_info);

  segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kDefaultModelSource);
  EXPECT_TRUE(segment_info_);
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());
  EXPECT_EQ(0, segment_info_->model_version());

  // Update model_version of segment_info
  created_segment_info.set_model_version(7);
  segment_info_cache_->UpdateSegmentInfo(kSegmentId, kDefaultModelSource,
                                         created_segment_info);

  segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kDefaultModelSource);
  EXPECT_TRUE(segment_info_);
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());
  EXPECT_EQ(7, segment_info_->model_version());

  // Unknown model source
  created_segment_info = CreateSegment(kSegmentId, kUnknownModelSource);
  segment_info_cache_->UpdateSegmentInfo(kSegmentId, kServerModelSource,
                                         created_segment_info);

  segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kServerModelSource);
  EXPECT_TRUE(segment_info_);
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());
  EXPECT_EQ(0, segment_info_->model_version());
  EXPECT_EQ(kServerModelSource, segment_info_->model_source());

  // Update model_version of segment_info
  created_segment_info.set_model_version(2);
  segment_info_cache_->UpdateSegmentInfo(kSegmentId, kServerModelSource,
                                         created_segment_info);

  segment_info_ =
      segment_info_cache_->GetSegmentInfo(kSegmentId, kServerModelSource);
  EXPECT_TRUE(segment_info_);
  EXPECT_EQ(kSegmentId, segment_info_->segment_id());
  EXPECT_EQ(2, segment_info_->model_version());
  // Deleting a non existing entry.
  segment_info_cache_->UpdateSegmentInfo(kSegmentId3, kServerModelSource,
                                         std::nullopt);
}

TEST_F(SegmentInfoCacheTest, GetSegmentInfoForBothModelsWithEmptyDatabase) {
  auto segments_found = segment_info_cache_->GetSegmentInfoForBothModels(
      {kSegmentId, kSegmentId2});
  EXPECT_TRUE(segments_found.get()->empty());
}

}  // namespace segmentation_platform