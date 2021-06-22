// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/test_segment_info_database.h"

#include <algorithm>

#include "base/metrics/metrics_hashes.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

namespace test {

TestSegmentInfoDatabase::TestSegmentInfoDatabase()
    : SegmentInfoDatabase(nullptr) {}

TestSegmentInfoDatabase::~TestSegmentInfoDatabase() = default;

void TestSegmentInfoDatabase::Initialize(SuccessCallback callback) {
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::GetAllSegmentInfo(
    AllSegmentInfoCallback callback) {
  std::move(callback).Run(segment_infos_);
}

void TestSegmentInfoDatabase::GetSegmentInfo(OptimizationTarget segment_id,
                                             SegmentInfoCallback callback) {
  auto result = std::find_if(
      segment_infos_.begin(), segment_infos_.end(),
      [segment_id](std::pair<OptimizationTarget, proto::SegmentInfo> pair) {
        return pair.first == segment_id;
      });

  std::move(callback).Run(result == segment_infos_.end()
                              ? absl::nullopt
                              : absl::make_optional(result->second));
}

void TestSegmentInfoDatabase::UpdateSegment(
    OptimizationTarget segment_id,
    absl::optional<proto::SegmentInfo> segment_info,
    SuccessCallback callback) {
  if (segment_info.has_value()) {
    proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
    info->CopyFrom(segment_info.value());
  } else {
    // Delete the segment.
    auto new_end = std::remove_if(
        segment_infos_.begin(), segment_infos_.end(),
        [segment_id](
            const std::pair<OptimizationTarget, proto::SegmentInfo>& pair) {
          return pair.first == segment_id;
        });
    segment_infos_.erase(new_end, segment_infos_.end());
  }
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::SaveSegmentResult(OptimizationTarget segment_id,
                                                proto::PredictionResult* result,
                                                SuccessCallback callback) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  if (result == nullptr) {
    info->clear_prediction_result();
  } else {
    auto* mutable_result = info->mutable_prediction_result();
    mutable_result->set_result(result->result());
    mutable_result->set_timestamp_us(result->timestamp_us());
  }
  std::move(callback).Run(true);
}

void TestSegmentInfoDatabase::AddUserActionFeature(
    OptimizationTarget segment_id,
    const std::string& user_action_name,
    int64_t length,
    proto::Aggregation aggregation) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  proto::SegmentationModelMetadata* metadata = info->mutable_model_metadata();
  proto::Feature* feature = metadata->add_features();
  proto::UserActionFeature* user_action = feature->mutable_user_action();
  user_action->set_user_action_hash(base::HashMetricName(user_action_name));
  feature->set_length(length);
  feature->set_aggregation(aggregation);
}

void TestSegmentInfoDatabase::AddPredictionResult(OptimizationTarget segment_id,
                                                  float score,
                                                  base::Time timestamp) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  proto::PredictionResult* result = info->mutable_prediction_result();
  result->set_result(score);
  result->set_timestamp_us(
      timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void TestSegmentInfoDatabase::AddDiscreteMapping(OptimizationTarget segment_id,
                                                 float mappings[][2],
                                                 int num_pairs) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  auto* discrete_mappings_map =
      info->mutable_model_metadata()->mutable_discrete_mappings();
  auto& discrete_mappings =
      (*discrete_mappings_map)[kSegmentationDiscreteMappingKey];
  for (int i = 0; i < num_pairs; i++) {
    auto* pair = mappings[i];
    auto* entry = discrete_mappings.add_entries();
    entry->set_min_result(pair[0]);
    entry->set_rank(pair[1]);
  }
}

void TestSegmentInfoDatabase::SetBucketDuration(OptimizationTarget segment_id,
                                                int64_t bucket_duration,
                                                proto::TimeUnit time_unit) {
  proto::SegmentInfo* info = FindOrCreateSegment(segment_id);
  info->mutable_model_metadata()->set_bucket_duration(bucket_duration);
  info->mutable_model_metadata()->set_time_unit(time_unit);
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
    info->set_segment_id(segment_id);
  }

  return info;
}

}  // namespace test

}  // namespace segmentation_platform
