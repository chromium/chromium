// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/test_segment_info_database.h"

#include "base/metrics/metrics_hashes.h"

namespace segmentation_platform {

namespace test {

TestSegmentInfoDatabase::TestSegmentInfoDatabase() = default;

TestSegmentInfoDatabase::~TestSegmentInfoDatabase() = default;

void TestSegmentInfoDatabase::GetAllSegmentInfo(
    AllSegmentInfoCallback callback) {
  std::move(callback).Run(segment_infos_);
}

void TestSegmentInfoDatabase::AddUserAction(
    OptimizationTarget segment_id,
    const std::string& user_action_name) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  proto::SegmentationModelMetadata* metadata = info->mutable_model_metadata();
  proto::Feature* feature = metadata->add_features();
  proto::UserActionFeature* user_action = feature->mutable_user_action();
  user_action->set_user_action_hash(base::HashMetricName(user_action_name));
}

proto::SegmentInfo* TestSegmentInfoDatabase::FindOrCreateSegment(
    OptimizationTarget segment_id) {
  proto::SegmentInfo* info = nullptr;
  for (auto& pair : segment_infos_) {
    if (pair.first == segment_id) {
      info = &pair.second;
      break;
    }
  }

  if (info == nullptr) {
    segment_infos_.emplace_back(segment_id, proto::SegmentInfo());
    info = &segment_infos_.back().second;
  }

  return info;
}

}  // namespace test

}  // namespace segmentation_platform
